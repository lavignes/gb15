#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include <gb15/gb15.h>

typedef struct RenderState {
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    u32 last_time;
} RenderState;

void vblank_callback(GB15State *state, void *userdata) {
    RenderState *render_state = userdata;
    SDL_Renderer *renderer = render_state->renderer;
    SDL_Texture *texture = render_state->texture;
    void *pixels;
    int pitch;
    SDL_LockTexture(texture, NULL, &pixels, &pitch);
    memcpy(pixels, state->lcd, sizeof(u32) * 23040);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    u32 delta_time = SDL_GetTicks() - render_state->last_time;
    if (delta_time <= 16) {
        SDL_Delay(16 - delta_time);
    }
    render_state->last_time = SDL_GetTicks();
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("GB15", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 160 * 4, 144 * 4, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);

    RenderState render_state;
    render_state.renderer = renderer;
    render_state.texture = texture;
    render_state.last_time = SDL_GetTicks();

    FILE *file = fopen("opus5.gb", "rb");
    fseek(file, 0, SEEK_END);
    uz size = (uz)ftell(file);
    rewind(file);
    u8 *rom = malloc(size);
    fread(rom, 1, size, file);
    fclose(file);

    GB15State *state = calloc(1, sizeof(GB15State));
    gb15_boot(state);

//    u32 cycles = 0;
//    u32 last_time = SDL_GetTicks();
    while (true) {
//        cycles++;
//        if (SDL_GetTicks() - last_time >= 1000) {
//            printf("cps: %d\n", cycles);
//            cycles = 0;
//            last_time = SDL_GetTicks();
//        }

        SDL_Event event;
//        SDL_PollEvent(&event);
        if (event.type == SDL_QUIT) {
            break;
        }
        gb15_tick(state, rom, vblank_callback, &render_state);
    }

    free(state);
    free(rom);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}