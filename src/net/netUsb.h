// USB network device glue for the DSpico firmware.
// Everything here except the queue accessors runs from the main loop.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "netQueue.h"

#ifdef __cplusplus
extern "C" {
#endif

// True while network mode owns the USB controller. Read by dcd_rp2040.c.
extern volatile bool gNetUsbActive;

// Set by the card handlers, acted on by net_usbTask.
extern volatile bool gNetUsbStartRequested;
extern volatile bool gNetUsbStopRequested;

// Frames from the host waiting for the DS. Producer: main loop. Consumer: card IRQ.
extern netq_t gNetHostToDs;
// Frames from the DS waiting for the host. Producer: card IRQ. Consumer: main loop.
extern netq_t gNetDsToHost;

// Sticky; cleared by the status command.
extern volatile bool gNetRxOverflow;

// Whether the DS asked for the slot IRQ line to follow the receive queue.
extern volatile bool gNetIrqEnabled;

// Host has configured the device (tud_ready), cached by the main loop.
extern volatile bool gNetLinkUp;

/// @brief Sets up the queues. Call once at boot.
void net_usbInit(void);

/// @brief Starts and stops USB as requested and moves frames. Call on every main loop pass.
void net_usbTask(void);

/// @brief Drives the slot IRQ line from the receive queue and \c gNetIrqEnabled.
void net_updateIrqLine(void);

#ifdef __cplusplus
}
#endif
