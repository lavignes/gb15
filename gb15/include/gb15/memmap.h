#ifndef _GB15_MEMMAP_H_
#define _GB15_MEMMAP_H_

#include <gb15/types.h>

typedef struct GB15MemMap {
    /**
     * 0x8000 Video RAM (Two banks on Gameboy Color)
     */
    u8 vram[2][8192]; // 8KB

    /**
     * 0xA000 Optional cart extension RAM
     */
    u8 cram[8192]; // 8KB

    /**
     * 0xC000 Onboard Working RAM
     */
    u8 wram[8192]; // 8KB

    /**
     * 0xD000 Switchable RAM banks (on Gameboy Color)
     */
    u8 sram[8][8192]; // 8KB

    /**
     * 0xE000 ECHO, OAM, IO, Virtual Registers and Zero Page
     */
    u8 himem[8192]; // 8KB

    /**
     * MBC
     */
    u8 mbc_version;
    u8 cart_register[8];

} GB15MemMap;

typedef enum GB15VirtualRegister {

    /**
     * Port/Mode Registers
     */
    GB15_REG_P1 =    0xFF00,
    GB15_REG_SB =    0xFF01,
    GB15_REG_SC =    0xFF02,
    GB15_REG_DIV =   0xFF03,
    GB15_REG_TIMA =  0xFF04,
    GB15_REG_TMA =   0xFF05,
    GB15_REG_TAC =   0xFF06,
    GB15_REG_KEY1 =  0xFF4D,
    GB15_REG_RP =    0xFF56,

    /**
     * Bank Control Registers
     */
    GB15_REG_VBK =   0xFF4F,
    GB15_REG_SVBK =  0xFF70,
    GB15_REG_BIOS =  0xFF50,

    /**
     * Interrupt Flags
     */
    GB15_REG_IF =    0xFF0F,
    GB15_REG_IE =    0xFFFF,

    /**
     * LCD Display Registers
     */
    GB15_REG_LCDC =  0xFF40,
    GB15_REG_STAT =  0xFF41,
    GB15_REG_SCY =   0xFF42,
    GB15_REG_SCX =   0xFF43,
    GB15_REG_LY =    0xFF44,
    GB15_REG_LYC =   0xFF45,
    GB15_REG_DMA =   0xFF46,
    GB15_REG_BGP =   0xFF47,
    GB15_REG_OBP0 =  0xFF48,
    GB15_REG_OBP1 =  0xFF49,
    GB15_REG_WY =    0xFF4A,
    GB15_REG_WX =    0xFF4B,
    GB15_REG_HDMA1 = 0xFF51,
    GB15_REG_HDMA2 = 0xFF52,
    GB15_REG_HDMA3 = 0xFF53,
    GB15_REG_HDMA4 = 0xFF54,
    GB15_REG_HDMA5 = 0xFF55,
    GB15_REG_BCPS =  0xFF68,
    GB15_REG_BCPD =  0xFF69,
    GB15_REG_OCPS =  0xFF6A,
    GB15_REG_OCPD =  0xFF6B,

    /**
     * Sound 1
     */
    GB15_REG_NR10 =  0xFF10,
    GB15_REG_NR11 =  0xFF11,
    GB15_REG_NR12 =  0xFF12,
    GB15_REG_NR13 =  0xFF13,
    GB15_REG_NR14 =  0xFF14,

    /**
     * Sound 2
     */
    GB15_REG_NR20 =  0xFF15,
    GB15_REG_NR21 =  0xFF16,
    GB15_REG_NR22 =  0xFF17,
    GB15_REG_NR23 =  0xFF18,
    GB15_REG_NR24 =  0xFF19,

    /**
     * Sound 3
     */
    GB15_REG_NR30 =  0xFF1A,
    GB15_REG_NR31 =  0xFF1B,
    GB15_REG_NR32 =  0xFF1C,
    GB15_REG_NR33 =  0xFF1D,
    GB15_REG_NR34 =  0xFF1E,

    /**
     * Sound 4
     */
    GB15_REG_NR40 =  0xFF1F,
    GB15_REG_NR41 =  0xFF20,
    GB15_REG_NR42 =  0xFF21,
    GB15_REG_NR43 =  0xFF22,
    GB15_REG_NR44 =  0xFF23,

    /**
     * Sound Control
     */
    GB15_REG_NR50 =  0xFF24,
    GB15_REG_NR51 =  0xFF25,
    GB15_REG_NR52 =  0xFF26,

} GB15VirtualRegister;

GB15_EXTERN u8 gb15_memmap_read(GB15MemMap *memmap, u8 *rom, u16 address);
GB15_EXTERN u8 gb15_memmap_write(GB15MemMap *memmap, u16 address, u8 value);

#endif /* _GB15_MEMMAP_H_ */