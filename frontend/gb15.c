#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include <gb15/gb15.h>

typedef struct RenderState {
    SDL_Renderer *renderer;
    SDL_Texture *texture;
} RenderState;

void hblank(GB15State *state, void *userdata) {
    RenderState *render_state = userdata;
    SDL_Renderer *renderer = render_state->renderer;
    SDL_Texture *texture = render_state->texture;
    void *pixels;
    int pitch;
    SDL_LockTexture(texture, NULL, &pixels, &pitch);
    memcpy(pixels, state->screen, sizeof(u32) * 23040);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("GB15", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 160 * 3, 144 * 3, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);

    RenderState render_state;
    render_state.renderer = renderer;
    render_state.texture = texture;

    FILE *file = fopen("cpu_instrs.gb", "rb");
    fseek(file, 0, SEEK_END);
    uz size = (uz)ftell(file);
    rewind(file);
    u8 *rom = malloc(size);
    fread(rom, 1, size, file);
    fclose(file);

//    u8 rom[0x4000] = {
//        0x3E, 0xE4,       // ld a, 0xE4
//        0xE0, 0x47,       // ldh (0x47), a  ; BGP = a
//        0x3E, 0x81,       // ld a, 0x81
//        0xE0, 0x40,       // ldh (0x40), a  ; LCDC = a
//        0x3E, 0x02,       // ld a, 0x02
//        0xE0, 0x41,       // ldh (0x41), a  ; STAT = a
//        0x21, 0x00, 0x88, // ld hl, 0x8800  ; hl = vram address
//        0x3E, 0xF0,       // ld a, 0xF0
//        0x22,             // ld (hl+), a
//        0x22,             // ld (hl+), a
//        0x3E, 0x01,       // ld a, 0x01
//        0x3C,             // inc a          ; 0x0015
//        0xE0, 0x42,       // ldh (0x42), a  ; SCY = a
//        0x3D,             // dec a
//        0x3C,             // inc a
//        0xE0, 0x43,       // ldh (0x43), a  ; SCX = a
//        0xC3, 0x15, 0x00, // jp 0x0014
//        0x10,             // STOP
//    };
//    uz size = 0x4000;

//    u8 rom[0x4000] = {
//        0x3E, 0xE4,       // ld a, 0xE4
//        0xE0, 0x47,       // ldh (0x47), a  ; BGP = a
//        0x3E, 0x81,       // ld a, 0x81
//        0xE0, 0x40,       // ldh (0x40), a  ; LCDC = a
//        0x3E, 0x02,       // ld a, 0x02
//        0xE0, 0x41,       // ldh (0x41), a  ; STAT = a
//        0x3C,             // inc a
////        0x47,             // ld b, a
////        0xE0, 0x44,       // ldh a, (0x44)  ; a = LY
////        0x3C,             // inc a
////        0x3C,             // inc a
//
////        0x80,             // add b
////        0x0E, 0x03,       // ld c, 0x03
////        0xCB, 0x29,       // sra c
////        0xE0, 0x42,       // ldh (0x42), a  ; SCY = a
////        0xE0, 0x43,       // ldh (0x43), a  ; SCX = a
//        0xC3, 0x0C, 0x00, // jp 0x000C
//        0x10,             // STOP
//    };
//    uz size = 0x4000;

    GB15State *state = calloc(1, sizeof(GB15State));
    gb15_boot(state, rom, size);
//
//    u8 vram[48] = {
//        0b00111100,
//        0b00111100,
//        0b01100110,
//        0b01100110,
//        0b01101110,
//        0b01101110,
//        0b01111110,
//        0b01111110,
//        0b01110110,
//        0b01110110,
//        0b01100110,
//        0b01100110,
//        0b00111100,
//        0b00111100,
//        0b00000000,
//        0b00000000,
//
//        0b00011000,
//        0b00000000,
//        0b00111000,
//        0b00000000,
//        0b01111000,
//        0b00000000,
//        0b00011000,
//        0b00000000,
//        0b00011000,
//        0b00000000,
//        0b00011000,
//        0b00000000,
//        0b00011000,
//        0b00000000,
//        0b00000000,
//        0b00000000,
//
//        0b00000000,
//        0b00111100,
//        0b00000000,
//        0b01100110,
//        0b00000000,
//        0b00000110,
//        0b00000000,
//        0b00001100,
//        0b00000000,
//        0b00011000,
//        0b00000000,
//        0b00110000,
//        0b00000000,
//        0b01111110,
//        0b00000000,
//        0b00000000,
//    };
//    for (u8 i = 0; i < 48; i++) {
//        gb15_memmap_write(&state->memmap, i + (u16)0x8800, vram[i]);
//    }
//
//    for (u8 i = 0; i < 32; i++) {
//        for (u8 j = 0; j < 20; j++) {
//            gb15_memmap_write(&state->memmap, i*(u8)32 + j + (u16)0x9800, j + i);
//        }
//    }

    do {
        SDL_Event event;
        SDL_PollEvent(&event);
        if (event.type == SDL_QUIT) {
            break;
        }
        gb15_tick(state, hblank, &render_state);
//    } while (!state->stopped);
    } while (true);

    free(state);
    free(rom);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}