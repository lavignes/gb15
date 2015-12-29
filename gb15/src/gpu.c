#include <string.h>

#include <gb15/gpu.h>
#include <gb15/gb15.h>

static inline s8 signify8(u8 value) {
    if (value <= (u8)0x7F) {
        return value;
    }
    return -(s8)(((~value) + (u8)1) & (u8)0xFF);
}

void gb15_gpu_init(GB15State *state)
{
    memset(state->gpu.lcd, 0xFF, sizeof(u32) * 23040);
}

static u8 bg_palette_for_data(u8 data, u8 bgp) {
    switch (data) {
        case 0x00:
            return bgp & (u8)0x03;
        case 0x01:
            return (bgp & (u8)0x0C) >> (u8)2;
        case 0x02:
            return (bgp & (u8)0x30) >> (u8)4;
        case 0x03:
            return (bgp & (u8)0xC0) >> (u8)6;
        default:
            break;
    }
    return 0;
}

static u32 bg_pixel_at(u8 x, u8 y, GB15Mmu *mmu, u8 lcdc, u8 scx, u8 scy, u8 bgp) {
    u16 bg_tile_map_table = (lcdc & 0x08)? (u16)0x9C00 : (u16)0x9800;
    u8 tile_x = (x + scx) >> (u8)3; // / 8;
    u8 tile_y = (y + scy) >> (u8)3; // / 8
    u16 tile_idx = ((u16)tile_y << (u16)5) + (u16)tile_x; // * 32 + tile_x
    u8 tile_code = gb15_mmu_read(mmu, NULL, bg_tile_map_table + tile_idx);
    u16 bg_pattern_table = (lcdc & (u8)0x10)? ((u16)0x8000 + tile_code * (u16)16) : (u16)((s16)0x8800 + signify8(tile_code) * (u16)16);
    u8 char_x = (x + scx) & (u8)0x07;            // % 8
    u8 char_y = ((y + scy) & (u8)0x07) << (u8)1; // % 8 * 2
    u8 bitlow = (u8)((gb15_mmu_read(mmu, NULL, bg_pattern_table + char_y) & ((u8)0x80 >> char_x)) != (u8)0);
    u8 bithigh = (u8)((gb15_mmu_read(mmu, NULL, bg_pattern_table + char_y + (u16)1) & ((u8)0x80 >> char_x)) != (u8)0);
    switch (bg_palette_for_data((bithigh << (u8)1) | bitlow, bgp)) {
        case 0x00:
            return 0xFFFFFFFF;
        case 0x01:
            return 0xAAAAAAFF;
        case 0x02:
            return 0x555555FF;
        case 0x03:
            return 0x000000FF;
        default:
            break;
    }
    return 0xFF0000FF;
}

void gb15_gpu_tick(GB15State *state, u8 *rom, GB15VBlankCallback vblank, void *userdata) {
    GB15Cpu *cpu = &state->cpu;
    GB15Gpu *gpu = &state->gpu;
    GB15Mmu *mmu = &state->mmu;
    u8 stat = mmu->io[GB15_IO_STAT];
    u8 mode = stat & (u8)0x03;
    u8 ly = mmu->io[GB15_IO_LY];
    u8 lcdc = mmu->io[GB15_IO_LCDC];
    u8 ie = mmu->io[GB15_IO_IE];
    switch (mode) {
        case 0x00: // HBlank
            if (gpu->tclocks < 204) {
                break;
            }
            if (ly == 143) {
                mode = 0x01;
                vblank(state, userdata);
                // Raise vblank
                if (cpu->ime && (ie & (u8)0x01)) {
                    mmu->io[GB15_IO_IF] |= (u8)0x01; //  vblank
                }
            } else {
                mode = 0x02;
            }
            ly++;
            gpu->tclocks = 0;
            break;

        case 0x01: // VBlank
            if (gpu->tclocks < 4560) {
                break;
            }
            if (ly > 153) {
                ly = 0;
                mode = 0x2;
            } else {
                ly++;
                gpu->tclocks = 0;
            }
            break;

        case 0x02: // OAM
            if (gpu->tclocks < 80) {
                break;
            }
            gpu->tclocks = 0;
            mode = 0x03;
            break;

        case 0x03: // LCD
            if (gpu->tclocks < 172) {
                break;
            }
            gpu->tclocks = 0;
            mode = 0x00;
            if (ly > 143) {
                break;
            }
            if (lcdc & 0x80) {
                if (lcdc & 0x01) {
                    u8 scx = mmu->io[GB15_IO_SCX];
                    u8 scy = mmu->io[GB15_IO_SCY];
                    u8 bgp = mmu->io[GB15_IO_BGP];
                    for (u8 x = 0; x < 160; x++) {
                        gpu->lcd[ly * 160 + x] = bg_pixel_at(x, ly, mmu, lcdc, scx, scy, bgp);
                    }
                }
            }
            break;

        default:
            break;
    }
    bool coincidence = (ly == mmu->io[GB15_IO_LYC]);
    // STAT interrupts
    if (cpu->ime && (ie & (u8)0x02)) {
        // Raise LY==LYC coincidence
        if (coincidence && (stat & (u8)0x04)) {
            mmu->io[GB15_IO_IF] |= (u8)0x02;
        }
        // Raise mode change TODO: This could me made shorter
        if ((stat & (u8)0x03) != mode) {
            switch (mode) {
                default:
                case 0x00:
                    if (stat & (u8)0x08) {
                        mmu->io[GB15_IO_IF] |= (u8)0x02;
                    }
                    break;
                case 0x01:
                    if (stat & (u8)0x10) {
                        mmu->io[GB15_IO_IF] |= (u8)0x02;
                    }
                    break;
                case 0x02:
                    if (stat & (u8)0x20) {
                        mmu->io[GB15_IO_IF] |= (u8)0x02;
                    }
                    break;
            }
        }
    }
    stat = (stat & ~(u8)0x04) | (coincidence << 6);
    stat = (stat & ~(u8)0x03) | mode;
    mmu->io[GB15_IO_LY] = ly;
    mmu->io[GB15_IO_STAT] = stat;
    gpu->tclocks++;
}