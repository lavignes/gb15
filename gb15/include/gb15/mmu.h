#ifndef _GB15_MEMMAP_H_
#define _GB15_MEMMAP_H_

#include <gb15/types.h>

typedef struct GB15Mmu {
    /**
     * 0x8000-0x9FFF Video RAM (Two banks on Gameboy Color)
     */
    u8 vram[2][8192]; // 8KB

    /**
     * 0xA000-0xBFFF Optional cart extension RAM
     */
    u8 cram[8192]; // 8KB

    /**
     * 0xC000-0xCFFF Onboard Working RAM
     */
    u8 wram[8192]; // 8KB

    /**
     * 0xD000-0xDFFF Switchable RAM banks (on Gameboy Color)
     */
    u8 sram[8][8192]; // 8KB

    /**
     * 0xFE00-0xFE9F OAM Table
     */
    u8 oam[160];

    /**
     * 0xFF00-0xFF7F IO. Extended to 0xFFFF to hold IE Register
     */
    u8 io[256];

    /**
     * 0xFF80-0xFFFE HI RAM
     */
    u8 hram[128];

    /**
     * MBC Version
     */
    u8 mbc_version;

} GB15Mmu;

typedef enum GB15IOPort {

    /**
     * Port/Mode Registers
     */
    GB15_IO_JOYP =  0x00,
    GB15_IO_SB =    0x01,
    GB15_IO_SC =    0x02,
    GB15_IO_DIV =   0x03,
    GB15_IO_TIMA =  0x04,
    GB15_IO_TMA =   0x05,
    GB15_IO_TAC =   0x06,
    GB15_IO_KEY1 =  0x4D,
    GB15_IO_RP =    0x56,

    /**
     * Bank Control Registers
     */
    GB15_IO_VBK =   0x4F,
    GB15_IO_SVBK =  0x70,
    GB15_IO_BIOS =  0x50,

    /**
     * Interrupt Flags
     */
    GB15_IO_IF =    0x0F,
    GB15_IO_IE =    0xFF,

    /**
     * LCD Display Registers
     */
    GB15_IO_LCDC =  0x40,
    GB15_IO_STAT =  0x41,
    GB15_IO_SCY =   0x42,
    GB15_IO_SCX =   0x43,
    GB15_IO_LY =    0x44,
    GB15_IO_LYC =   0x45,
    GB15_IO_DMA =   0x46,
    GB15_IO_BGP =   0x47,
    GB15_IO_OBP0 =  0x48,
    GB15_IO_OBP1 =  0x49,
    GB15_IO_WY =    0x4A,
    GB15_IO_WX =    0x4B,
    GB15_IO_HDMA1 = 0x51,
    GB15_IO_HDMA2 = 0x52,
    GB15_IO_HDMA3 = 0x53,
    GB15_IO_HDMA4 = 0x54,
    GB15_IO_HDMA5 = 0x55,
    GB15_IO_BCPS =  0x68,
    GB15_IO_BCPD =  0x69,
    GB15_IO_OCPS =  0x6A,
    GB15_IO_OCPD =  0x6B,

    /**
     * Sound 1
     */
    GB15_IO_NR10 =  0x10,
    GB15_IO_NR11 =  0x11,
    GB15_IO_NR12 =  0x12,
    GB15_IO_NR13 =  0x13,
    GB15_IO_NR14 =  0x14,

    /**
     * Sound 2
     */
    GB15_IO_NR20 =  0x15,
    GB15_IO_NR21 =  0x16,
    GB15_IO_NR22 =  0x17,
    GB15_IO_NR23 =  0x18,
    GB15_IO_NR24 =  0x19,

    /**
     * Sound 3
     */
    GB15_IO_NR30 =  0x1A,
    GB15_IO_NR31 =  0x1B,
    GB15_IO_NR32 =  0x1C,
    GB15_IO_NR33 =  0x1D,
    GB15_IO_NR34 =  0x1E,

    /**
     * Sound 4
     */
    GB15_IO_NR40 =  0x1F,
    GB15_IO_NR41 =  0x20,
    GB15_IO_NR42 =  0x21,
    GB15_IO_NR43 =  0x22,
    GB15_IO_NR44 =  0x23,

    /**
     * Sound Control
     */
    GB15_IO_NR50 =  0x24,
    GB15_IO_NR51 =  0x25,
    GB15_IO_NR52 =  0x26,

} GB15IOPort;

GB15_EXTERN u8 gb15_mmu_read(GB15Mmu *mmu, u8 *rom, u16 address);
GB15_EXTERN u8 gb15_mmu_write(GB15Mmu *mmu, u16 address, u8 value);

#endif /* _GB15_MEMMAP_H_ */