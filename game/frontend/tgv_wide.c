#include "game/frontend/tgv_wide.h"

#include <string.h>

void tgv_wide_scale(const uint8_t *pixels, int w, int h, uint8_t *out, int width, int height)
{
    if (pixels == NULL || out == NULL || w <= 0 || h <= 0 || width <= 0 || height <= 0) return;
    const int64_t scaled = (int64_t)h * width / w;            /* the picture's height at the screen's width */
    const int64_t first = (scaled - height) / 2;              /* the scaled row at the screen's top (negative: bars) */
    for (int y = 0; y < height; y++) {
        const int64_t row = first + y;
        uint8_t *to = out + (size_t)y * (size_t)width;
        if (row < 0 || row >= scaled) {
            memset(to, 0, (size_t)width);
            continue;
        }
        const uint8_t *from = pixels + (size_t)((2 * row + 1) * h / (2 * scaled)) * (size_t)w;
        for (int x = 0; x < width; x++) to[x] = from[(int)((2 * (int64_t)x + 1) * w / (2 * (int64_t)width))];
    }
}
