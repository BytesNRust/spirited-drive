/* The menus' backdrop on a wide screen (backdrop.h). Port glue: nothing here is the original's. */
#include "game/sys/backdrop.h"

#include "game/viewer/screen_layout.h"

#include <stdint.h>
#include <string.h>

enum { SOURCE_W = ORIGINAL_WIDTH, HEIGHT = SCREEN_HEIGHT, COLOURS = 256, BRIGHTNESS_PERCENT = 45 };

void backdrop_shade_table(const uint8_t rgb[768], uint8_t out[256])
{
    for (int c = 0; c < COLOURS; c++) {
        int want[3];
        for (int k = 0; k < 3; k++) want[k] = (rgb[3 * c + k] * BRIGHTNESS_PERCENT + 50) / 100;
        int best = 0;
        long best_gap = -1;
        for (int i = 0; i < COLOURS; i++) {
            long gap = 0;
            for (int k = 0; k < 3; k++) gap += (long)(rgb[3 * i + k] - want[k]) * (rgb[3 * i + k] - want[k]);
            if (best_gap < 0 || gap < best_gap) best = i, best_gap = gap;
        }
        out[c] = (uint8_t)best;
    }
}

void backdrop_compose(const uint8_t *src, uint8_t *dst, int dst_w, const uint8_t shade[256])
{
    if (dst_w <= SOURCE_W) {
        memcpy(dst, src, (size_t)SOURCE_W * HEIGHT);
        return;
    }
    const int crop_top = screen_cockpit_scale(dst_w).crop_top;
    for (int y = 0; y < HEIGHT; y++) {
        const int sy = (2 * (y + crop_top) + 1) * SOURCE_W / (2 * dst_w);  /* the pixel's centre, in src */
        const uint8_t *row = src + (size_t)sy * SOURCE_W;
        for (int x = 0; x < dst_w; x++) dst[(size_t)y * dst_w + x] = shade[row[(2 * x + 1) * SOURCE_W / (2 * dst_w)]];
    }
    const int left = screen_menu_left(dst_w);
    for (int y = 0; y < HEIGHT; y++) memcpy(dst + (size_t)y * dst_w + left, src + (size_t)y * SOURCE_W, SOURCE_W);
}
