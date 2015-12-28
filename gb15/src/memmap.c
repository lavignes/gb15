#include <gb15/memmap.h>
#include <gb15/bios.h>

static u8 mbc0_read(GB15MemMap *memmap, u8 *rom, u16 address) {
    switch (address) {
        case 0x0000 ... 0x7FFF:
            if (memmap->io[GB15_IO_BIOS] == 0x00 && address < (u16)0x0100) {
                return GB15_BIOS[address];
            }
            return rom[address];
        case 0x8000 ... 0x9FFF:
            return memmap->vram[memmap->io[GB15_IO_VBK] & 0x01][address - (u16)0x8000];
        case 0xA000 ... 0xBFFF:
            return memmap->cram[address - (u16)0xA000];
        case 0xC000 ... 0xCFFF:
            return memmap->wram[address - (u16)0xC000];
        case 0xD000 ... 0xDFFF:
            return memmap->sram[memmap->io[GB15_IO_VBK] & 0x07][address - (u16)0xD000];
        case 0xE000 ... 0xFDFF:
            return mbc0_read(memmap, rom, address - (u16)0x1000);
        case 0xFE00 ... 0xFE9F:
            return memmap->oam[address - (u16)0xFE00];
        case 0xFF00 ... 0xFF7F:
        case 0xFFFF:
            return memmap->io[address - (u16)0xFF00];
        case 0xFF80 ... 0xFFFE:
            return memmap->io[address - (u16)0xFF80];
        default:
            break;
    }
    return 0;
}

static u8 mbc0_write(GB15MemMap *memmap, u16 address, u8 value) {
    switch (address) {
        case 0x8000 ... 0x9FFF:
            return memmap->vram[memmap->io[GB15_IO_VBK] & 0x01][address - (u16)0x8000] = value;
        case 0xA000 ... 0xBFFF:
            return memmap->cram[address - (u16)0xA000] = value;
        case 0xC000 ... 0xCFFF:
            return memmap->wram[address - (u16)0xC000] = value;
        case 0xD000 ... 0xDFFF:
            return memmap->sram[memmap->io[GB15_IO_VBK] & 0x07][address - (u16)0xD000] = value;
        case 0xE000 ... 0xFDFF:
            return mbc0_write(memmap, address - (u16)0x1000, value);
        case 0xFE00 ... 0xFE9F:
            return memmap->oam[address - (u16)0xFE00] = value;
        case 0xFF00 ... 0xFF7F:
        case 0xFFFF:
            return memmap->io[address - (u16)0xFF00] = value;
        case 0xFF80 ... 0xFFFE:
            return memmap->io[address - (u16)0xFF80] = value;
        default:
            break;
    }
    return 0;
}

u8 gb15_memmap_read(GB15MemMap *memmap, u8 *rom, u16 address) {
    return mbc0_read(memmap, rom, address);
}

u8 gb15_memmap_write(GB15MemMap *memmap, u16 address, u8 value) {
    return mbc0_write(memmap, address, value);
}
