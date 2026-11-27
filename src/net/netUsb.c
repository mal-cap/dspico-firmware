// TinyUSB CDC-NCM glue for DSpico network mode.
//
// Threading: TinyUSB is not thread-safe. tud_task() and every tud_network_*
// call happen here, in the main loop. The card handlers only touch the two
// queues, the request flags and gNetLinkUp.
#include "common.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "tusb.h"
#include "powerSaving.h"
#include "dspico_net_proto.h"
#include "netUsb.h"

#define NET_HOST_TO_DS_SLOTS 8
#define NET_DS_TO_HOST_SLOTS 4

volatile bool gNetUsbActive;
volatile bool gNetUsbStartRequested;
volatile bool gNetUsbStopRequested;
volatile bool gNetRxOverflow;
volatile bool gNetIrqEnabled;
volatile bool gNetLinkUp;

netq_t gNetHostToDs;
netq_t gNetDsToHost;

static netq_slot_t sHostToDsSlots[NET_HOST_TO_DS_SLOTS];
static netq_slot_t sDsToHostSlots[NET_DS_TO_HOST_SLOTS];

// MAC address of the host's end of the link (sent in the iMACAddress string).
// Locally administered. The DS keeps its own MAC for the frames it sends.
uint8_t tud_network_mac_address[6] = { 0x02, 0x44, 0x53, 0x50, 0x49, 0x01 };

// usbd.c in 0.17.0 calls dcd_init(rhport); the vendored DCD takes the newer
// (rhport, rh_init) form. usbd.c is compiled with -Ddcd_init=net_dcdInitCompat.
bool dcd_init(uint8_t rhport, const void *rh_init); // rh_init is unused
void net_dcdInitCompat(uint8_t rhport)
{
    dcd_init(rhport, NULL);
}

static void resetQueues(void)
{
    netq_init(&gNetHostToDs, sHostToDsSlots, NET_HOST_TO_DS_SLOTS);
    netq_init(&gNetDsToHost, sDsToHostSlots, NET_DS_TO_HOST_SLOTS);
    gNetRxOverflow = false;
}

void net_usbInit(void)
{
    resetQueues();
#ifdef NET_AUTOSTART
    gNetUsbStartRequested = true;
#endif
}

// Called from both the main loop and the card IRQ.
void __time_critical_func(net_updateIrqLine)(void)
{
    uint32_t irq = save_and_disable_interrupts();
    gpio_put(PIN_IRQ, gNetUsbActive && gNetIrqEnabled && !netq_empty(&gNetHostToDs));
    restore_interrupts(irq);
}

static void start(void)
{
    if (gNetUsbActive)
    {
        return;
    }
    resetQueues();
    pwr_disableAfterBootPowerSaving(); // USB RAM powered, USB clocks awake
    pwr_disableUsbPowerSaving();       // USB PLL + clk_usb running
    gNetUsbActive = true;              // before init so DCD events reach usbd
    // tusb_init() would do nothing without CFG_TUSB_RHPORT0_MODE.
    tud_init(0);                       // dcd_init (connects), IRQ on
}

static void stop(void)
{
    if (!gNetUsbActive)
    {
        return;
    }
    tud_deinit(0);                     // disconnect, remove IRQ, reset block
    gNetUsbActive = false;
    gNetLinkUp = false;
    gNetIrqEnabled = false;
    net_updateIrqLine();
    pwr_enableUsbPowerSaving();
}

void net_usbTask(void)
{
    if (gNetUsbStopRequested)
    {
        gNetUsbStopRequested = false;
        stop();
    }
    if (gNetUsbStartRequested)
    {
        gNetUsbStartRequested = false;
        start();
    }
    if (!gNetUsbActive)
    {
        return;
    }

    tud_task();
    gNetLinkUp = tud_ready();

    // Host -> DS: hand pending datagrams over as queue space allows.
    tud_network_recv_renew();

    // DS -> host. tud_network_xmit copies synchronously via xmit_cb.
    for (;;)
    {
        netq_slot_t *slot = netq_peek(&gNetDsToHost);
        if (!slot || !gNetLinkUp || !tud_network_can_xmit(slot->length))
        {
            break;
        }
        tud_network_xmit(slot, slot->length);
        netq_pop(&gNetDsToHost);
    }
}

// A datagram from the host. Returning false keeps it in the NTB; it is
// offered again on the next tud_network_recv_renew().
bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    if (size < 14 || size > NETQ_FRAME_MAX)
        return true; // malformed: drop
    if (!netq_push(&gNetHostToDs, src, size))
    {
        gNetRxOverflow = true;
        return false;
    }
    net_updateIrqLine();
    return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    netq_slot_t *slot = (netq_slot_t *)ref;
    memcpy(dst, slot->data, arg);
    return arg;
}
