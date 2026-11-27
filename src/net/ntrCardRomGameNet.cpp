// Card command handlers for the network commands (EC-EF), unscrambled game
// mode. Same Cmd0/Cmd1 split as ntrCardRomGameUsb.cpp. Placed in main SRAM
// (__time_critical_func): scratch Y is full.
//
// These run in the PIO interrupt. They never call TinyUSB and never copy a
// frame: TX payloads land directly in a queue slot, RX payloads are DMA'd
// straight out of one (netq_slot_t's {length, reserved} header is the
// NET_RX block header).
#include "common.h"
#include "ntrCardRom.h"
#include "ntrCardRomGameNoScramble.h"
#include "dspico_net_proto.h"
#include "netUsb.h"
#include "ntrCardRomGameNet.h"

static_assert(NETQ_SLOT_BYTES >= DSPICO_NET_BLOCK_LARGE_BYTES, "slot must hold a large block");
static_assert(offsetof(netq_slot_t, data) == DSPICO_NET_RX_HEADER_BYTES, "slot header must match NET_RX header");

// Source for NET_RX replies with no frame (or a frame too big for the block),
// and sink for NET_TX payloads that have nowhere to go.
static u32 sNetScratch[DSPICO_NET_BLOCK_LARGE_BYTES / 4];
// The RX slot just sent is popped at the next network command, once the DMA
// of the previous transfer has certainly finished.
static volatile bool sRxPopPending;
static netq_slot_t* volatile sTxSlot;

static inline u32 __time_critical_func(blockBytes)(u32 cmd0)
{
    return ((cmd0 >> 16) & 1) ? DSPICO_NET_BLOCK_LARGE_BYTES : DSPICO_NET_BLOCK_SMALL_BYTES;
}

static inline void __time_critical_func(finishPendingRxPop)(void)
{
    if (sRxPopPending)
    {
        sRxPopPending = false;
        netq_pop(&gNetHostToDs);
        net_updateIrqLine();
    }
}

// EC: NET_CTRL (no payload)

extern "C" void __time_critical_func(ntrc_gameNetCtrlCmd0)(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio)
{
    ntrc_finishGameNoScrambleCmd0(romEmu);
}

extern "C" void __time_critical_func(ntrc_gameNetCtrlCmd1)(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio)
{
    ntrc_noPayload(pio);
    ntrc_finishGameNoScrambleCmd1(romEmu);
    switch ((romEmu->cmd0 >> 16) & 0xFF)
    {
        case DSPICO_NET_CTRL_INIT:
            if (ntrc_isUsbActive())
            {
                break;
            }
            // The main loop starts USB; TinyUSB is not IRQ-safe.
            finishPendingRxPop();
            sTxSlot = nullptr;
            gNetIrqEnabled = false;
            gNetUsbStartRequested = true;
            break;
        case DSPICO_NET_CTRL_DEINIT:
            finishPendingRxPop();
            sTxSlot = nullptr;
            gNetUsbStopRequested = true;
            break;
        case DSPICO_NET_CTRL_IRQ_ON:
            gNetIrqEnabled = true;
            net_updateIrqLine();
            break;
        case DSPICO_NET_CTRL_IRQ_OFF:
            gNetIrqEnabled = false;
            net_updateIrqLine();
            break;
    }
}

// EF: NET_STATUS (4 bytes to the DS)
// Answered at Cmd0, like READ_ID, so the word is in the FIFO long before
// the DS clocks it out.

extern "C" void __time_critical_func(ntrc_gameNetStatusCmd0)(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio)
{
    u32 flags = 0;
    u32 count = 0;
    u32 length = 0;
    if (gNetUsbActive)
    {
        finishPendingRxPop();
        flags |= DSPICO_NET_ST_ACTIVE;
        if (gNetLinkUp)
        {
            flags |= DSPICO_NET_ST_LINK_UP;
        }
        if (!netq_full(&gNetDsToHost))
        {
            flags |= DSPICO_NET_ST_TX_READY;
        }
        if (gNetRxOverflow)
        {
            flags |= DSPICO_NET_ST_RX_OVERFLOW;
            gNetRxOverflow = false;
        }
        count = netq_count(&gNetHostToDs);
        netq_slot_t* slot = netq_peek(&gNetHostToDs);
        if (slot)
        {
            length = slot->length;
        }
    }
    ntrc_beginWrite(pio, 4);
    ntrc_writeWord(pio, DSPICO_NET_ST_MAKE(flags, count, length));
    ntrc_finishGameNoScrambleCmd0(romEmu);
}

extern "C" void __time_critical_func(ntrc_gameNetStatusCmd1)(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio)
{
    ntrc_finishGameNoScrambleCmd1(romEmu);
}

// EE: NET_RX (one block to the DS)

extern "C" void __time_critical_func(ntrc_gameNetRxCmd0)(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio)
{
    const u32 bytes = blockBytes(romEmu->cmd0);
    const void* src = sNetScratch;
    sNetScratch[0] = 0; // length 0, flags 0: nothing queued

    if (gNetUsbActive)
    {
        finishPendingRxPop();
        netq_slot_t* slot = netq_peek(&gNetHostToDs);
        if (slot)
        {
            if (slot->length + DSPICO_NET_RX_HEADER_BYTES > bytes)
            {
                sNetScratch[0] = DSPICO_NET_RX_FLAG_TOO_BIG << 16; // left queued
            }
            else
            {
                slot->reserved = 0; // flags
                src = slot;
                sRxPopPending = true;
            }
        }
    }

    ntrc_beginWrite(pio, bytes);
    ntrc_dmaToBus(src, bytes);
    ntrc_finishGameNoScrambleCmd0(romEmu);
}

extern "C" void __time_critical_func(ntrc_gameNetRxCmd1)(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio)
{
    ntrc_finishGameNoScrambleCmd1(romEmu);
}

// ED: NET_TX (one block from the DS)

extern "C" void __time_critical_func(ntrc_gameNetTxCmd0)(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio)
{
    ntrc_finishGameNoScrambleCmd0(romEmu);
}

// cmd0 bytes: [31:24] ED, [23:16] flags (bit0 large, bit1 chunk), [15:8] chunk
// index, [7:0] bit0 last chunk. cmd1 [15:0]: frame length.
static inline bool __time_critical_func(isChunk)(u32 cmd0) { return (cmd0 >> 16) & DSPICO_NET_TX_CHUNK; }

static void __time_critical_func(netTxPayloadComplete)(ntr_rom_emu_t* romEmu)
{
    netq_slot_t* slot = sTxSlot;
    if (!slot)
    {
        return;
    }
    const u32 cmd0 = romEmu->cmd0;
    const u32 length = romEmu->cmd1 & 0xFFFF;
    if (isChunk(cmd0))
    {
        const u32 index = (cmd0 >> 8) & 0xFF;
        if (!(cmd0 & DSPICO_NET_TX_CHUNK_LAST))
        {
            return; // more chunks to come into this slot
        }
        sTxSlot = nullptr;
        if (length < DSPICO_NET_FRAME_MIN || length > DSPICO_NET_FRAME_MAX ||
            length > (index + 1) * DSPICO_NET_BLOCK_SMALL_BYTES)
        {
            return; // not committed: dropped
        }
    }
    else
    {
        sTxSlot = nullptr;
        if (length < DSPICO_NET_FRAME_MIN || length > DSPICO_NET_FRAME_MAX || length > blockBytes(cmd0))
        {
            return;
        }
    }
    slot->length = (u16)length;
    netq_commit(&gNetDsToHost);
    // The main loop's __wfi() returns after this interrupt and sends it.
}

extern "C" void __time_critical_func(ntrc_gameNetTxCmd1)(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio)
{
    const u32 cmd0 = romEmu->cmd0;
    u32* dest = sNetScratch;
    u32 bytes;
    if (isChunk(cmd0))
    {
        // 512-byte chunks of one frame, in order. Chunk 0 takes a fresh slot
        // (an unfinished earlier frame is simply overwritten: never committed).
        const u32 index = (cmd0 >> 8) & 0xFF;
        bytes = DSPICO_NET_BLOCK_SMALL_BYTES;
        if (index == 0)
        {
            sTxSlot = gNetUsbActive ? netq_reserve(&gNetDsToHost) : nullptr;
        }
        netq_slot_t* slot = sTxSlot;
        if (slot && index < NETQ_SLOT_BYTES / DSPICO_NET_BLOCK_SMALL_BYTES)
        {
            dest = (u32*)(slot->data + index * DSPICO_NET_BLOCK_SMALL_BYTES);
        }
        else
        {
            sTxSlot = nullptr;
        }
    }
    else
    {
        bytes = blockBytes(cmd0);
        netq_slot_t* slot = gNetUsbActive ? netq_reserve(&gNetDsToHost) : nullptr;
        sTxSlot = slot;
        if (slot)
        {
            dest = (u32*)slot->data;
        }
    }
    ntrc_beginRead(pio, bytes);
    ntrc_finishGameNoScrambleCmd1WithReadPayload(romEmu, dest, bytes, netTxPayloadComplete);
}

extern "C" void ntrc_resetNet(void)
{
    sRxPopPending = false;
    sTxSlot = nullptr;
    gNetIrqEnabled = false;
    if (gNetUsbActive)
    {
        gNetUsbStopRequested = true;
    }
    net_updateIrqLine();
}
