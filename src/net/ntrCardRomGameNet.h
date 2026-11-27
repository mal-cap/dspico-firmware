#pragma once
#include "common.h"
#include "ntrCardRom.h"

#ifdef __cplusplus
extern "C" {
#endif

void ntrc_gameNetCtrlCmd0(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio);
void ntrc_gameNetCtrlCmd1(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio);
void ntrc_gameNetTxCmd0(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio);
void ntrc_gameNetTxCmd1(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio);
void ntrc_gameNetRxCmd0(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio);
void ntrc_gameNetRxCmd1(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio);
void ntrc_gameNetStatusCmd0(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio);
void ntrc_gameNetStatusCmd1(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio);

/// Tears network mode down on a console reset.
void ntrc_resetNet(void);

#ifdef __cplusplus
}
#endif
