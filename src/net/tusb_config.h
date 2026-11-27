// TinyUSB configuration for the device stack (usbd + NCM) compiled from the
// SDK's TinyUSB 0.17.0 in network mode. The vendored DCD in the firmware's
// src/tinyusb/ keeps its own tusb_config.h; the settings both read must agree
// (MCU, OS, endpoint 0 size, speed).
#pragma once

#define CFG_TUSB_MCU           OPT_MCU_RP2040
#define CFG_TUSB_OS            OPT_OS_NONE
#define CFG_TUSB_DEBUG         0
#define CFG_TUD_ENABLED        1
#define CFG_TUD_MAX_SPEED      OPT_MODE_DEFAULT_SPEED
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN     __attribute__((aligned(4)))
#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_NCM    1
#define CFG_TUD_CDC    0
#define CFG_TUD_MSC    0
#define CFG_TUD_HID    0
#define CFG_TUD_MIDI   0
#define CFG_TUD_VENDOR 0
#define CFG_TUD_ECM_RNDIS 0

// Full speed, one Ethernet frame per NTB is plenty; two buffers each way.
#define CFG_TUD_NCM_IN_NTB_MAX_SIZE  2048
#define CFG_TUD_NCM_OUT_NTB_MAX_SIZE 2048
#define CFG_TUD_NCM_IN_NTB_N         2
#define CFG_TUD_NCM_OUT_NTB_N        2
