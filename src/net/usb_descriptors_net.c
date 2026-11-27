// USB descriptors for DSpico network mode (CDC-NCM), after TinyUSB's
// net_lwip_webserver example.
#include "tusb.h"

// pid.codes test PID; a real one is still to be requested.
#define USB_VID 0x1209
#define USB_PID 0x0001

enum { ITF_NUM_CDC = 0, ITF_NUM_CDC_DATA, ITF_NUM_TOTAL };
enum { STRID_LANGID = 0, STRID_MANUFACTURER, STRID_PRODUCT, STRID_SERIAL, STRID_INTERFACE, STRID_MAC };

#define EPNUM_NET_NOTIF 0x81
#define EPNUM_NET_OUT   0x02
#define EPNUM_NET_IN    0x82

static const tusb_desc_device_t sDeviceDescriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = STRID_MANUFACTURER,
    .iProduct = STRID_PRODUCT,
    .iSerialNumber = STRID_SERIAL,
    .bNumConfigurations = 1,
};

const uint8_t *tud_descriptor_device_cb(void)
{
    return (const uint8_t *)&sDeviceDescriptor;
}

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_NCM_DESC_LEN)

static const uint8_t sConfigDescriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
    TUD_CDC_NCM_DESCRIPTOR(ITF_NUM_CDC, STRID_INTERFACE, STRID_MAC, EPNUM_NET_NOTIF, 64,
                           EPNUM_NET_OUT, EPNUM_NET_IN, CFG_TUD_NET_ENDPOINT_SIZE, CFG_TUD_NET_MTU),
};

const uint8_t *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return sConfigDescriptor;
}

static const char *const sStrings[] = {
    [STRID_MANUFACTURER] = "DSpico",
    [STRID_PRODUCT] = "DSpico network adapter",
    [STRID_SERIAL] = "000001",
    [STRID_INTERFACE] = "DSpico NCM",
};

static uint16_t sStringBuffer[32];

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    unsigned count = 0;
    (void)langid;

    if (index == STRID_LANGID)
    {
        sStringBuffer[1] = 0x0409;
        count = 1;
    }
    else if (index == STRID_MAC)
    {
        // The host-side MAC as 12 upper-case hex digits.
        for (unsigned i = 0; i < 6; i++)
        {
            sStringBuffer[1 + count++] = "0123456789ABCDEF"[(tud_network_mac_address[i] >> 4) & 0xF];
            sStringBuffer[1 + count++] = "0123456789ABCDEF"[tud_network_mac_address[i] & 0xF];
        }
    }
    else if (index < sizeof(sStrings) / sizeof(sStrings[0]) && sStrings[index])
    {
        const char *s = sStrings[index];
        while (s[count] && count < 31)
        {
            sStringBuffer[1 + count] = (uint16_t)s[count];
            count++;
        }
    }
    else
    {
        return NULL;
    }

    sStringBuffer[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * count + 2));
    return sStringBuffer;
}
