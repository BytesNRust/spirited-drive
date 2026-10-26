/* platform_present through the texture's locked memory against the buffer-and-upload path and a per-pixel reference. SDL's
 * dummy video driver and its software renderer: what the renderer drew is read back with SDL_RenderReadPixels.
 * Included, not linked, to reach the renderer. */
#include "platform/sdl3/platform_sdl3.c"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static uint32_t *readback(void)
{
    SDL_Surface *s = SDL_RenderReadPixels(renderer, NULL);
    if (s == NULL) return NULL;
    SDL_Surface *c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_XRGB8888);
    SDL_DestroySurface(s);
    if (c == NULL) return NULL;
    uint32_t *out = malloc((size_t)c->w * (size_t)c->h * 4);
    if (out != NULL)
        for (int y = 0; y < c->h; y++) memcpy(out + (size_t)y * (size_t)c->w, (uint8_t *)c->pixels + (size_t)y * (size_t)c->pitch, (size_t)c->w * 4);
    SDL_DestroySurface(c);
    return out;
}

static void present_case(int width, int height, int threads, unsigned seed)
{
    PlatformConfig config = { width, height, "test", 0, 0, 0 };
    CHECK(platform_init(&config));
    uint8_t *px = malloc((size_t)width * (size_t)height);
    uint32_t *want = malloc((size_t)width * (size_t)height * 4);
    PlatformColor palette[256];
    for (int i = 0; i < 256; i++)
        palette[i] = (PlatformColor){ (uint8_t)(i * 7 + seed), (uint8_t)(i * 13 + 5 * seed), (uint8_t)(255 - i + seed) };
    platform_set_present_threads(threads);
    for (int frame = 0; frame < 3; frame++) {
        for (size_t i = 0; i < (size_t)width * height; i++) px[i] = (uint8_t)(((i + (size_t)frame * 977) * 2654435761u) >> 23);
        for (size_t i = 0; i < (size_t)width * height; i++)
            want[i] = (uint32_t)palette[px[i]].r << 16 | (uint32_t)palette[px[i]].g << 8 | palette[px[i]].b;
        for (int lock = 1; lock >= 0; lock--) {   /* locked texture, then the buffer and upload */
            platform_set_present_lock(lock);
            platform_present(px, palette);
            uint32_t *got = readback();
            CHECK(got != NULL);
            CHECK(got != NULL && memcmp(got, want, (size_t)width * height * 4) == 0);
            free(got);
        }
    }
    platform_set_present_lock(1);
    free(px), free(want);
    platform_shutdown();
}

/* platform_present_split: each pixel through the palette its mask value names, on the locked texture and on the buffer */
static void split_case(int width, int height, int threads, unsigned seed)
{
    PlatformConfig config = { width, height, "test", 0, 0, 0 };
    CHECK(platform_init(&config));
    const size_t count = (size_t)width * (size_t)height;
    uint8_t *px = malloc(count), *mask = malloc(count);
    uint32_t *want = malloc(count * 4);
    PlatformColor palette[256], world[256], half[256];
    for (int i = 0; i < 256; i++) {
        palette[i] = (PlatformColor){ (uint8_t)(i * 7 + seed), (uint8_t)(i * 13 + 5 * seed), (uint8_t)(255 - i + seed) };
        world[i] = (PlatformColor){ (uint8_t)(palette[i].r / 2), (uint8_t)(palette[i].g / 3), (uint8_t)(palette[i].b / 5) };
        half[i] = (PlatformColor){ (uint8_t)((palette[i].r + world[i].r) / 2), (uint8_t)((palette[i].g + world[i].g) / 2),
                                   (uint8_t)((palette[i].b + world[i].b) / 2) };
    }
    platform_set_present_threads(threads);
    for (size_t i = 0; i < count; i++) {
        px[i] = (uint8_t)((i * 2654435761u) >> 23);
        mask[i] = (uint8_t)(((i * 40503u) >> 7) % 3);
        const PlatformColor c = mask[i] == 1 ? world[px[i]] : mask[i] == 2 ? half[px[i]] : palette[px[i]];
        want[i] = (uint32_t)c.r << 16 | (uint32_t)c.g << 8 | c.b;
    }
    for (int lock = 1; lock >= 0; lock--) {
        platform_set_present_lock(lock);
        platform_present_split(px, palette, world, half, mask);
        uint32_t *got = readback();
        CHECK(got != NULL);
        CHECK(got != NULL && memcmp(got, want, count * 4) == 0);
        free(got);
    }
    platform_set_present_lock(1);
    free(px), free(mask), free(want);
    platform_shutdown();
}

int main(void)
{
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    unsetenv("TNFS_CAPTURE_DIR");
    /* render scale 1 and 2 of the 21:9 and 4:3 frames, odd sizes too; bands 0 and 4 */
    const int sizes[][2] = { { 640, 480 }, { 1120, 480 }, { 2240, 960 }, { 203, 97 } };
    for (int k = 0; k < 4; k++)
        for (int threads = 0; threads <= 4; threads += 4) present_case(sizes[k][0], sizes[k][1], threads, (unsigned)k);
    for (int k = 0; k < 4; k++)
        for (int threads = 0; threads <= 4; threads += 4) split_case(sizes[k][0], sizes[k][1], threads, (unsigned)k);
    return TEST_REPORT();
}
