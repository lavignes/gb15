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

static u32 bg_pixel_at(GB15Mmu *mmu, u8 x, u8 y, u8 lcdc, u8 scx, u8 scy, u8 bgp) {
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
    return 0x000000FF;
}

void gb15_gpu_tick(GB15State *state, u8 *rom, GB15VBlankCallback vblank, void *userdata) {
    GB15Gpu *gpu = &state->gpu;
    GB15Mmu *mmu = &state->mmu;
    u8 lcdc = mmu->io[GB15_IO_LCDC];
    if ((lcdc & (u8)0x80) == (u8)0x00) {
        mmu->io[GB15_IO_LY] = 0x00;
        mmu->io[GB15_IO_STAT] &= ~(u8)0x03;
        gpu->clocks = 456;
        return;
    }
    u8 stat = mmu->io[GB15_IO_STAT];
    u8 ly = mmu->io[GB15_IO_LY];
    u8 mode;
    if (ly >= 144) {
        mode = 0x01;
        gpu->stat_raised = false;
    } else if (gpu->clocks >= 456 - 80) {
        mode = 0x02;
        gpu->stat_raised = false;
    } else if (gpu->clocks >= 456 - 80 - 172) {
        mode = 0x03;
        gpu->stat_raised = false;
    } else {
        mode = 0x00;
        if (!gpu->stat_raised && (stat & (u8)0x08)) {
            mmu->io[GB15_IO_IF] |= (u8)0x02; // stat
            gpu->stat_raised = true;
        }
    }
    gpu->clocks--;
    if (gpu->clocks == 0) {
        gpu->clocks = 456;
        ly++;
        if (ly == 144) {
            if (!gpu->vblank_raised) {
                mmu->io[GB15_IO_IF] |= (u8)0x01; // vblank
                if (stat & (u8)0x10) {
                    mmu->io[GB15_IO_IF] |= (u8)0x02; // stat
                }
                gpu->vblank_raised = true;
            }
            vblank(state, userdata);
        } else if (ly > 153) {
            gpu->vblank_raised = false;
            ly = 0;
        }
        if (ly == mmu->io[GB15_IO_LYC]) {
            stat |= (u8)0x04;
            if (stat & (u8)0x40) {
                mmu->io[GB15_IO_IF] |= (u8)0x02;
            }
        } else {
            stat &= ~(u8)0x04;
        }
        if (ly < 144) {
            if (lcdc & 0x01) {
                u8 scx = mmu->io[GB15_IO_SCX];
                u8 scy = mmu->io[GB15_IO_SCY];
                u8 bgp = mmu->io[GB15_IO_BGP];
                for (u8 x = 0; x < 160; x++) {
                    gpu->lcd[ly * 160 + x] = bg_pixel_at(mmu, x, ly, lcdc, scx, scy, bgp);
                }
            }
        }
    }
    stat = (stat & ~(u8)0x03) | mode;
    mmu->io[GB15_IO_LY] = ly;
    mmu->io[GB15_IO_STAT] = stat;
}