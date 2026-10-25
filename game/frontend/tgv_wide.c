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

int tgv_wide_fills(int w, int h, int width, int height)
{
    if (w <= 0 || h <= 0 || width <= 0 || height <= 0) return 1;
    const int64_t scaled = (int64_t)h * width / w;
    return (scaled - height) * 100 <= (int64_t)TGV_WIDE_MAX_CROP_PERCENT * height;
}

void tgv_wide_compose(const uint8_t *pixels, int w, int h, uint8_t *out, int width, int height, const uint8_t shade[256])
{
    if (pixels == NULL || out == NULL || shade == NULL || tgv_wide_fills(w, h, width, height)) {
        tgv_wide_scale(pixels, w, h, out, width, height);
        return;
    }
    const int64_t scaled = (int64_t)h * width / w;            /* the enlarged copy's height at the screen's width */
    const int64_t first = (scaled - height) / 2;
    const int64_t fitted = (int64_t)w * height / h;           /* the picture's width at the screen's height */
    const int64_t left = (width - fitted) / 2;
    for (int y = 0; y < height; y++) {
        uint8_t *to = out + (size_t)y * (size_t)width;
        const uint8_t *side = pixels + (size_t)((2 * (first + y) + 1) * h / (2 * scaled)) * (size_t)w;
        for (int x = 0; x < width; x++) to[x] = shade[side[(int)((2 * (int64_t)x + 1) * w / (2 * (int64_t)width))]];
        const uint8_t *from = pixels + (size_t)((2 * (int64_t)y + 1) * h / (2 * (int64_t)height)) * (size_t)w;
        for (int x = 0; x < fitted; x++) to[left + x] = from[(int)((2 * (int64_t)x + 1) * w / (2 * fitted))];
    }
}
