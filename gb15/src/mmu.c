#include <gb15/mmu.h>
#include <gb15/bios.h>

static u8 mbc0_read(GB15Mmu *mmu, u8 *rom, u16 address) {
    switch (address) {
        case 0x0000 ... 0x7FFF:
            if (mmu->io[GB15_IO_BIOS] == 0x00 && address < (u16)0x0100) {
                return GB15_BIOS[address];
            }
            return rom[address];
        case 0x8000 ... 0x9FFF:
            return mmu->vram[mmu->io[GB15_IO_VBK] & 0x01][address - (u16)0x8000];
        case 0xA000 ... 0xBFFF:
            return mmu->cram[address - (u16)0xA000];
        case 0xC000 ... 0xCFFF:
            return mmu->wram[address - (u16)0xC000];
        case 0xD000 ... 0xDFFF:
            return mmu->sram[mmu->io[GB15_IO_VBK] & 0x07][address - (u16)0xD000];
        case 0xE000 ... 0xFDFF:
            return mbc0_read(mmu, rom, address - (u16)0x1000);
        case 0xFE00 ... 0xFE9F:
            return mmu->oam[address - (u16)0xFE00];
        case 0xFF00 ... 0xFF7F:
        case 0xFFFF:
            return mmu->io[address - (u16)0xFF00];
        case 0xFF80 ... 0xFFFE:
            return mmu->io[address - (u16)0xFF80];
        default:
            break;
    }
    return 0;
}

static u8 mbc0_write(GB15Mmu *mmu, u16 address, u8 value) {
    switch (address) {
        case 0x8000 ... 0x9FFF:
            return mmu->vram[mmu->io[GB15_IO_VBK] & 0x01][address - (u16)0x8000] = value;
        case 0xA000 ... 0xBFFF:
            return mmu->cram[address - (u16)0xA000] = value;
        case 0xC000 ... 0xCFFF:
            return mmu->wram[address - (u16)0xC000] = value;
        case 0xD000 ... 0xDFFF:
            return mmu->sram[mmu->io[GB15_IO_VBK] & 0x07][address - (u16)0xD000] = value;
        case 0xE000 ... 0xFDFF:
            return mbc0_write(mmu, address - (u16)0x1000, value);
        case 0xFE00 ... 0xFE9F:
            return mmu->oam[address - (u16)0xFE00] = value;
        case 0xFF00 ... 0xFF7F:
        case 0xFFFF:
            return mmu->io[address - (u16)0xFF00] = value;
        case 0xFF80 ... 0xFFFE:
            return mmu->io[address - (u16)0xFF80] = value;
        default:
            break;
    }
    return 0;
}

u8 gb15_mmu_read(GB15Mmu *mmu, u8 *rom, u16 address) {
    return mbc0_read(mmu, rom, address);
}

u8 gb15_mmu_write(GB15Mmu *mmu, u16 address, u8 value) {
    return mbc0_write(mmu, address, value);
}
