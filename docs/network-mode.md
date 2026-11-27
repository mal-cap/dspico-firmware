# Network mode

In network mode the DSpico appears to the USB host as a CDC-NCM network adapter, and four card commands carry Ethernet frames between the DS and the host.

Protocol version 1. The constants are in [`src/net/dspico_net_proto.h`](../src/net/dspico_net_proto.h).

## Commands

The commands are in **unscrambled game mode**, using opcodes the firmware otherwise leaves as dummies. Command bytes are written most-significant first, as with the other commands. Timing is the same as for the USB commands: a 6.7 MHz clock, at least 4 latency cycles for reads (DSpico → DS) and at least 8 for writes (DS → DSpico).

| Command | Name | Payload | Description |
|---|---|---|---|
| `EC SS xx xx xx xx xx xx` | `NET_CTRL` | none | Sub-command `SS` (below) |
| `ED 00 xx xx 00 00 LL LL` | `NET_TX` | DS → DSpico, 512 bytes | Queues one Ethernet frame of `LLLL` bytes (at most 512) for the host |
| `ED 01 xx xx 00 00 LL LL` | `NET_TX` large | DS → DSpico, 2048 bytes | As `ED 00` with a 2048-byte block; see below |
| `ED 02 II 0L 00 00 LL LL` | `NET_TX` chunk | DS → DSpico, 512 bytes | Chunk `II` (0–3) of a frame of `LLLL` bytes; `L` = 1 on the last chunk |
| `EE 0K xx xx xx xx xx xx` | `NET_RX` | DSpico → DS, 512 or 2048 bytes | Pops the oldest frame from the host |
| `EF xx xx xx xx xx xx xx` | `NET_STATUS` | DSpico → DS, 4 bytes | Status word |

`K` is the block size: `0` = 512 bytes, `1` = 2048 bytes.

### `NET_CTRL` sub-commands

| `SS` | Name | Effect |
|---|---|---|
| `01` | `INIT` | Starts the USB network device with empty queues and the IRQ line off; ignored while running |
| `02` | `DEINIT` | Disconnects from USB and deasserts the IRQ line |
| `03` | `IRQ_ON` | Asserts the IRQ line while the receive queue is not empty |
| `04` | `IRQ_OFF` | Never asserts the IRQ line (polling); the default after `INIT` |

Network mode and the DS-driven USB mode (`E8 01`) are mutually exclusive. Starting one while the other is active is ignored.

### `NET_STATUS` word

Little-endian, as read by the DS.

| Bits | Name | Meaning |
|---|---|---|
| 0 | `LINK_UP` | The host has configured the USB device |
| 1 | `TX_READY` | The transmit queue has room for a frame |
| 2 | `RX_OVERFLOW` | A host frame was held back because the receive queue was full (cleared on read) |
| 3 | `ACTIVE` | Network mode is running |
| 8–15 | `RX_COUNT` | Frames waiting, saturating at 255 |
| 16–31 | `RX_LEN` | Length of the oldest waiting frame, 0 if none |

`NET_STATUS` is answered at Cmd0, like `READ_ID`, so polling it is cheap.

### `NET_RX` payload

```
offset 0  u16 length      little-endian; 0 = the queue was empty
offset 2  u16 flags       bit 0: the frame did not fit the requested block and was not popped
offset 4  frame bytes     Ethernet II, no FCS
...       zero padding to the block size
```

A 512-byte block carries frames up to 508 bytes, and a 2048-byte block carries any frame. The DS reads `NET_STATUS` first and picks the block size from `RX_LEN`. If the frame doesn't fit, the firmware sets flag bit 0, reports the real length, and leaves the frame queued.

### `NET_TX` payload

The frame starts at offset 0 and is zero-padded to 512 bytes. `LLLL` is the real length (14–1514). If the transmit queue is full, the frame is dropped; check `TX_READY` first.

**A single DS → DSpico write must not be larger than 512 bytes**: on hardware, payload bytes 512–519 of a 2048-byte write arrive as a copy of 504–511. Longer frames are sent as 512-byte chunks in order; chunk `II` goes to offset `II × 512`, the chunk with `L` = 1 commits the frame, and chunk 0 discards any unfinished one. The firmware also accepts `ED 01`, but on hardware such a frame arrives corrupted. Reads are not affected.

## Frames

Plain Ethernet II: destination MAC, source MAC, EtherType, payload; no FCS and no VLAN tag. The DSpico is a transparent bridge and does not rewrite addresses. The DS uses its own MAC address as the source.

## IRQ line

With `IRQ_ON`, the line is asserted while `RX_COUNT > 0` and released when the last frame is popped, in the same way as the USB event queue. Link changes don't raise it; poll `NET_STATUS`.

Leave it off while a retail game runs. Newer SDK ARM7 components treat any card IREQ as "card pulled out" and stop the game, unless bit 7 of header byte `0x1F` is set.

## Sharing the bus with a game

Network commands can arrive between any two complete SD operations. A sender in a running game should hold the SDK card lock (`0x027FFFE0`) for each command and use the same control words as Pico Loader's in-game SD commands, so the commands arrive unscrambled:

| Transfer | ROMCTRL |
|---|---|
| No data (`NET_CTRL`) | `0xA0406000` |
| Read 4 bytes (`NET_STATUS`) | `0xA7446000` |
| Read 512 / 2048 bytes (`NET_RX`) | `0xA1444000` / `0xA3444000` |
| Write 512 bytes (`NET_TX`, each chunk) | `0xE1486000` |

`NET_RX` pops the frame; there is no retry.

## Implementation

The code is in [`src/net/`](../src/net/).

**TinyUSB:**
- Network mode uses the device stack of TinyUSB **0.17.0** (`usbd`, `ncm_device`), compiled from `pico-sdk/lib/tinyusb`.
- The pico-sdk this firmware uses ships 0.15.0, so check out 0.17.0 there before building:
  ```sh
  git -C pico-sdk/lib/tinyusb fetch --depth 1 origin tag 0.17.0
  git -C pico-sdk/lib/tinyusb checkout 0.17.0
  ```
- The vendored DCD (`src/tinyusb/dcd_rp2040.c`) is shared with the DS-driven USB mode. It sends its events to TinyUSB only while network mode is active, and to the USB event queue otherwise.
- `usbd.c` is built with `-Ddcd_init=net_dcdInitCompat`, to adapt it to the DCD's `dcd_init` signature.

**Card handlers** run from RAM and never copy frames: `NET_TX` writes straight into a queue slot, and `NET_RX` is DMA'd straight out of one (each slot starts with the `{length, flags}` header) and popped at the next network command.

**Power saving:** game-mode power saving turns the USB RAM off; `INIT` turns it back on, as the DS-driven USB mode does.

**`NET_AUTOSTART=ON`** (CMake option): starts network mode at boot without a DS, to test enumeration with the cart on USB power only. Not for use in a console, because entering game mode powers USB down.
