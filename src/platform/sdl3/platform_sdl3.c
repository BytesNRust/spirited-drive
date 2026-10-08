#include "platform/platform.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint32_t *rgb_buffer;
static int fb_width;
static int fb_height;

bool platform_init(const PlatformConfig *config)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "platform: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    fb_width = config->width;
    fb_height = config->height;

    if (!SDL_CreateWindowAndRenderer(config->title, fb_width, fb_height, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        fprintf(stderr, "platform: window creation failed: %s\n", SDL_GetError());
        platform_shutdown();
        return false;
    }
    SDL_SetRenderLogicalPresentation(renderer, fb_width, fb_height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(renderer, 1);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, fb_width, fb_height);
    rgb_buffer = malloc((size_t)fb_width * (size_t)fb_height * sizeof *rgb_buffer);
    if (texture == NULL || rgb_buffer == NULL) {
        fprintf(stderr, "platform: framebuffer allocation failed: %s\n", SDL_GetError());
        platform_shutdown();
        return false;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return true;
}

void platform_shutdown(void)
{
    free(rgb_buffer);
    rgb_buffer = NULL;
    if (texture != NULL) SDL_DestroyTexture(texture);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    texture = NULL;
    renderer = NULL;
    window = NULL;
    SDL_Quit();
}

bool platform_poll_events(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) return false;
    }
    return true;
}

void platform_present(const uint8_t *pixels, const PlatformColor palette[256])
{
    uint32_t lut[256];
    for (int i = 0; i < 256; i++) {
        lut[i] = ((uint32_t)palette[i].r << 16) | ((uint32_t)palette[i].g << 8) | palette[i].b;
    }
    const size_t count = (size_t)fb_width * (size_t)fb_height;
    for (size_t i = 0; i < count; i++) {
        rgb_buffer[i] = lut[pixels[i]];
    }
    SDL_UpdateTexture(texture, NULL, rgb_buffer, fb_width * (int)sizeof *rgb_buffer);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

uint32_t platform_ticks_ms(void)
{
    return (uint32_t)SDL_GetTicks();
}

void platform_sleep_ms(uint32_t ms)
{
#ifdef __EMSCRIPTEN__
    /* With ASYNCIFY this unwinds to the browser so the tab stays responsive. */
    emscripten_sleep(ms);
#else
    SDL_Delay(ms);
#endif
}
