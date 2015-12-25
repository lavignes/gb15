#include <string.h>

#include <gb15/gpu.h>

static inline s8 signify8(u8 value) {
    if (value <= (u8)0x7F) {
        return value;
    }
    return -(s8)(((~value) + (u8)1) & (u8)0xFF);
}

void gb15_gpu_init(GB15State *state)
{
    memset(state->screen, 0xFF, sizeof(u32) * 23040);
    gb15_memmap_write(&state->memmap, GB15_REG_BGP, 0xFF);
}

static u8 bg_palette_for_data(u8 data, u8 bgp) {
    switch (data) {
        case 0b00:
            return bgp & (u8)0x03;
        case 0b01:
            return (bgp & (u8)0x0C) >> (u8)2;
        case 0b10:
            return (bgp & (u8)0x30 >> (u8)4);
        case 0b11:
            return (bgp & (u8)0xC0 >> (u8)6);
        default:
            break;
    }
    return 0;
}

#include <stdlib.h>

static u32 bg_pixel_at(u8 x, u8 y, GB15MemMap *memmap, u8 lcdc, u8 scx, u8 scy, u8 bgp) {
    scy = 0;
    u16 tile_chars_offset = (lcdc & 0b00001000)? (u16)0x9C00 : (u16)0x9800;
    u8 tile_x = (x + scx) / (u8)8;
    u8 tile_y = (y + scy) / (u8)8;
    u16 tile_idx = tile_y * (u8)32 + tile_x;
    u8 char_code = gb15_memmap_read(memmap, tile_chars_offset + tile_idx);
    u16 char_data_offset = (lcdc & (u8)0b00010000)? ((u16)0x8000 + char_code * (u16)16) : (u16)((s16)0x8800 + signify8(char_code) * (u16)16);
    u8 char_x = (x + scx) % (u8)8;
    u8 char_y = ((y + scy) % (u8)8) * (u8)2;
    u8 bitlow = (u8)((gb15_memmap_read(memmap, char_data_offset + char_y) & ((u8)1 << char_x)) != (u8)0);
    u8 bithigh = (u8)((gb15_memmap_read(memmap, char_data_offset + char_y + (u16)1) & ((u8)1 << char_x)) != (u8)0);
    switch (bg_palette_for_data((bithigh << (u8)1) | bitlow, bgp)) {
//    switch ((bithigh << (u8)1) | bitlow) {
        case 0b00:
            return 0xFFFFFFFF;
        case 0b01:
            return 0xAAAAAAFF;
        case 0b10:
            return 0x555555FF;
        case 0b11:
            return 0x000000FF;
        default:
            break;
    }
    return 0;
}

void gb15_gpu_tick(GB15State *state, GB15HBlankCallback hblank, void *userdata) {
    state->gpu_tclocks += state->tclocks;
    GB15MemMap *memmap = &state->memmap;
    u8 stat = gb15_memmap_read(memmap, GB15_REG_STAT);
    u8 mode = stat & (u8)0b11;
    u8 ly = gb15_memmap_read(memmap, GB15_REG_LY);
    u8 lcdc = gb15_memmap_read(memmap, GB15_REG_LCDC);
    switch (mode) {
        case 0x00: // HBlank
            if (state->gpu_tclocks < 204) {
                return;
            }
            state->gpu_tclocks = 0;
            if (ly == 143) {
                mode = 0x01;
                hblank(state, userdata);
            } else {
                mode = 0x02;
            }
            ly++;
            break;

        case 0x01: // VBlank
            if (state->gpu_tclocks < 4560) {
                return;
            }
            state->gpu_tclocks = 0;
            ly++;
            if (ly > 153) {
                ly = 0;
                mode = 0x2;
            }
            break;

        case 0x02: // OAM
            if (state->gpu_tclocks < 80) {
                return;
            }
            state->gpu_tclocks = 0;
            mode = 0x03;
            break;

        case 0x03: // LCD
            if (state->gpu_tclocks < 172) {
                return;
            }
            state->gpu_tclocks = 0;
            mode = 0x00;
            if (lcdc & 0b10000000) {
                if (lcdc & 0b00000001) {
                    if (ly > 143) {
                        break;
                    }
                    u8 scx = gb15_memmap_read(memmap, GB15_REG_SCX);
                    u8 scy = gb15_memmap_read(memmap, GB15_REG_SCY);
                    u8 bgp = gb15_memmap_read(memmap, GB15_REG_BGP);
                    for (u8 x = 0; x < 160; x ++) {
                        state->screen[ly * 160 + x] = bg_pixel_at(x, ly, memmap, lcdc, scx, scy, bgp);
                    }
                }
            }
            break;

        default:
            break;
    }
    stat = (stat & ~(u8)0b11) | mode;
    gb15_memmap_write(memmap, GB15_REG_LY, ly);
    gb15_memmap_write(memmap, GB15_REG_STAT, stat);
}