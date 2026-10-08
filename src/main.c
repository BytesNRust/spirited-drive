/* Temporary entry point: proves the platform layer end to end (window,
 * indexed framebuffer, palette, event loop) on every target before any
 * recovered game code lands. */
#include "platform/platform.h"

#include <stdlib.h>

enum { SCREEN_W = 640, SCREEN_H = 480 };

static void build_palette(PlatformColor palette[256])
{
    for (int i = 0; i < 256; i++) {
        palette[i] = (PlatformColor){ (uint8_t)i, (uint8_t)(255 - i), (uint8_t)((i * 4) & 0xff) };
    }
}

static void draw_test_pattern(uint8_t *pixels, uint32_t frame)
{
    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            pixels[y * SCREEN_W + x] = (uint8_t)((x ^ y) + frame);
        }
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const PlatformConfig config = { SCREEN_W, SCREEN_H, "The Need for Speed SE" };
    if (!platform_init(&config)) return EXIT_FAILURE;

    uint8_t *pixels = malloc(SCREEN_W * SCREEN_H);
    if (pixels == NULL) {
        platform_shutdown();
        return EXIT_FAILURE;
    }
    PlatformColor palette[256];
    build_palette(palette);

    for (uint32_t frame = 0; platform_poll_events(); frame++) {
        draw_test_pattern(pixels, frame);
        platform_present(pixels, palette);
        platform_sleep_ms(1);
    }

    free(pixels);
    platform_shutdown();
    return EXIT_SUCCESS;
}
