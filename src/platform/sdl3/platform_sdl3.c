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

enum { KEY_QUEUE_SIZE = 16, VBLANK_HZ = 60 };

/* TNFS_CAPTURE_DIR=dir [TNFS_CAPTURE_EVERY=n]: write every n-th presented
 * frame as dir/frame_NNNNN.ppm, to check the game's screens headless. */
static const char *capture_dir;
static int capture_every = 30;
static unsigned presented_frames;
static uint64_t next_vblank_ns;
static PlatformKey key_queue[KEY_QUEUE_SIZE];
static int key_head;
static int key_count;

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
    capture_dir = SDL_getenv("TNFS_CAPTURE_DIR");
    const char *every = SDL_getenv("TNFS_CAPTURE_EVERY");
    if (every != NULL && SDL_atoi(every) > 0) capture_every = SDL_atoi(every);

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

static PlatformKey translate_key(SDL_Keycode key)
{
    switch (key) {
    case SDLK_LEFT: return PLATFORM_KEY_LEFT;
    case SDLK_RIGHT: return PLATFORM_KEY_RIGHT;
    case SDLK_UP: return PLATFORM_KEY_UP;
    case SDLK_DOWN: return PLATFORM_KEY_DOWN;
    case SDLK_RETURN: return PLATFORM_KEY_ENTER;
    case SDLK_ESCAPE: return PLATFORM_KEY_ESCAPE;
    case SDLK_SPACE: return PLATFORM_KEY_SPACE;
    default: return PLATFORM_KEY_NONE;
    }
}

bool platform_poll_events(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) return false;
        if (event.type == SDL_EVENT_KEY_DOWN && key_count < KEY_QUEUE_SIZE) {
            const PlatformKey key = translate_key(event.key.key);
            if (key != PLATFORM_KEY_NONE) key_queue[(key_head + key_count++) % KEY_QUEUE_SIZE] = key;
        }
    }
    return true;
}

PlatformKey platform_next_key(void)
{
    if (key_count == 0) return PLATFORM_KEY_NONE;
    const PlatformKey key = key_queue[key_head];
    key_head = (key_head + 1) % KEY_QUEUE_SIZE;
    key_count--;
    return key;
}

uint8_t *platform_read_file(const char *path, size_t *size)
{
    uint8_t *data = SDL_LoadFile(path, size);
    if (data == NULL) fprintf(stderr, "platform: cannot read %s: %s\n", path, SDL_GetError());
    return data;
}

void platform_free_file(uint8_t *data)
{
    SDL_free(data);
}

static void capture_frame(void)
{
    if (capture_dir == NULL || presented_frames++ % (unsigned)capture_every != 0) return;
    char path[1024];
    SDL_snprintf(path, sizeof path, "%s/frame_%05u.ppm", capture_dir, presented_frames - 1);
    FILE *f = fopen(path, "wb");
    if (f == NULL) return;
    fprintf(f, "P6\n%d %d\n255\n", fb_width, fb_height);
    for (int i = 0; i < fb_width * fb_height; i++) {
        const uint8_t px[3] = { (uint8_t)(rgb_buffer[i] >> 16), (uint8_t)(rgb_buffer[i] >> 8), (uint8_t)rgb_buffer[i] };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

void platform_wait_vblank(void)
{
    const uint64_t period = SDL_NS_PER_SECOND / VBLANK_HZ;
    const uint64_t now = SDL_GetTicksNS();
    if (next_vblank_ns == 0 || now > next_vblank_ns + period) next_vblank_ns = now;  /* resync after stalls */
    next_vblank_ns += period;
    if (next_vblank_ns > now) platform_sleep_ms((uint32_t)((next_vblank_ns - now) / SDL_NS_PER_MS));
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
    capture_frame();
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
