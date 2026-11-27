cmake_minimum_required(VERSION 3.13)
#set(PICO_SDK_PATH "../FIRMWARE/pico-sdk") # enable this if vscode or cmake complains about PICO_SDK_PATH not being set
include(pico_sdk_import.cmake)
project(DSpico_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()
add_compile_definitions(DSpico
  PICO_STACK_SIZE=0x600
  PICO_CORE1_STACK_SIZE=0x400
  ENABLE_R4_MODE
  #DSPICO_ENABLE_WRFUXXED # This macro enables the WRFUXXED exploit to work.
  # ENABLE_PREVENT_DSI_AUTOBOOT # meant to be used with WRFU, doesn't work properly on the 3ds
)

add_executable(DSpico
  src/main.cpp
  src/common.h
  src/romData.S
  src/romData.h
  src/blowfish.c
  src/blowfish.h
  src/powerSaving.c
  src/powerSaving.h
  src/scrambler.c
  src/scrambler.h
  src/scramblerRing.c
  src/scramblerRing.h
  src/ntrCardRom.c
  src/ntrCardRom.h
  src/ntrCardRomNorm.c
  src/ntrCardRomSecure.c
  src/ntrCardRomGame.c
  src/ntrCardRomGame.h
  src/ntrCardRomGameNoScramble.c
  src/ntrCardRomGameNoScramble.h
  src/ntrCardRomGameSd.cpp
  src/ntrCardRomGameR4.cpp
  src/ntrCardRomGameUsb.cpp
  src/ntrCardRomGameUsb.h
  src/usbEventQueue.c
  src/usbEventQueue.h
  src/tinyusb/dcd_rp2040.c
  src/tinyusb/rp2040_usb.c
  src/tinyusb/rp2040_usb.h
  src/tinyusb/dcd.h
  src/tinyusb/tusb_option.h
  src/tinyusb/tusb_compiler.h
  src/tinyusb/tusb_common.h
  src/tinyusb/tusb_config.h
  src/tinyusb/tusb_mcu.h
  src/tinyusb/tusb_verify.h
  src/tinyusb/tusb_types.h
  src/tinyusb/tusb_debug.h
  src/tinyusb/osal.h
  src/tinyusb/osal_none.h
  src/ntrCardSpiUart.c
  src/ntrCardSpiUart.h
  src/wrfuxxed.h
  src/wrfuxxed.S
  src/xor.S
  src/xor.h
  src/ntrCardIrq.S
  src/r4.h
  src/sd/rp2040_sdio.cpp
  src/sd/rp2040_sdio.h
  src/sd/SdCardInfo.h
  src/sd/SdCardInfo.h
  src/sd/SdCard.cpp
  src/sd/fatfs/diskio.cpp
  src/sd/fatfs/diskio.h
  src/sd/fatfs/ff.c
  src/sd/fatfs/ff.h
  src/sd/fatfs/ffconf.h
  src/sd/fatfs/ffunicode.c
  src/sd/fatfs/ffsystem.c
)

if(EXISTS "${CMAKE_SOURCE_DIR}/roms/default.nds" AND EXISTS "${CMAKE_SOURCE_DIR}/roms/dsimode.nds")
  target_compile_definitions(DSpico PRIVATE
    DETECT_CONSOLE_TYPE # This macro enables the firmware to switch the rom based on which console is detected. You shouldn't change this.
  )
endif()

# Network mode: card commands EC-EF bridged to a USB CDC-NCM network adapter
# (docs/network-mode.md). Needs TinyUSB 0.17.0 in pico-sdk/lib/tinyusb.
set(DSPICO_NET_DIR ${CMAKE_CURRENT_LIST_DIR}/src/net)
set(TUSB_SRC ${PICO_SDK_PATH}/lib/tinyusb/src)
option(NET_AUTOSTART "Start network mode at boot, without the DS" OFF)
set(DSPICO_NET_TUSB_SOURCES
  ${TUSB_SRC}/tusb.c
  ${TUSB_SRC}/common/tusb_fifo.c
  ${TUSB_SRC}/device/usbd.c
  ${TUSB_SRC}/device/usbd_control.c
  ${TUSB_SRC}/class/net/ncm_device.c
)
target_sources(DSpico PRIVATE
  ${DSPICO_NET_DIR}/netUsb.c
  ${DSPICO_NET_DIR}/ntrCardRomGameNet.cpp
  ${DSPICO_NET_DIR}/usb_descriptors_net.c
  ${DSPICO_NET_TUSB_SOURCES}
)
# SDK TinyUSB first so its own sources resolve to its own headers; the only
# header they need from elsewhere is tusb_config.h (in DSPICO_NET_DIR).
target_include_directories(DSpico PRIVATE ${TUSB_SRC} ${CMAKE_CURRENT_LIST_DIR}/src ${DSPICO_NET_DIR})
set_source_files_properties(${TUSB_SRC}/device/usbd.c PROPERTIES COMPILE_DEFINITIONS "dcd_init=net_dcdInitCompat")
if(NET_AUTOSTART)
  target_compile_definitions(DSpico PRIVATE NET_AUTOSTART)
endif()

pico_generate_pio_header(DSpico ${CMAKE_CURRENT_LIST_DIR}/src/ntrCard.pio)
pico_generate_pio_header(DSpico ${CMAKE_CURRENT_LIST_DIR}/src/sd/rp2040_sdio.pio)

pico_enable_stdio_uart(DSpico 1)
pico_add_extra_outputs(DSpico)
target_link_libraries(DSpico pico_stdlib hardware_pio hardware_dma pico_multicore)
