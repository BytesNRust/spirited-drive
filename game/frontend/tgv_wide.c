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

void tgv_wide_compose(const uint8_t *pixels, int w, int h, uint8_t *out, int width, int height, const uint8_t shade[256],
                      int trim_left, int trim_right)
{
    if (pixels == NULL || out == NULL || shade == NULL || tgv_wide_fills(w, h, width, height)) {
        tgv_wide_scale(pixels, w, h, out, width, height);
        return;
    }
    if (trim_left < 0 || trim_right < 0 || trim_left + trim_right >= w) trim_left = trim_right = 0;
    const int kept = w - trim_left - trim_right;
    const int64_t scaled = (int64_t)h * width / kept;         /* the enlarged copy's height at the screen's width */
    const int64_t first = (scaled - height) / 2;
    const int64_t fitted = (int64_t)kept * height / h;        /* the picture's width at the screen's height */
    const int64_t left = (width - fitted) / 2;
    for (int y = 0; y < height; y++) {
        uint8_t *to = out + (size_t)y * (size_t)width;
        const uint8_t *side = pixels + (size_t)((2 * (first + y) + 1) * h / (2 * scaled)) * (size_t)w + trim_left;
        for (int x = 0; x < width; x++) to[x] = shade[side[(int)((2 * (int64_t)x + 1) * kept / (2 * (int64_t)width))]];
        const uint8_t *from = pixels + (size_t)((2 * (int64_t)y + 1) * h / (2 * (int64_t)height)) * (size_t)w + trim_left;
        for (int x = 0; x < fitted; x++) to[left + x] = from[(int)((2 * (int64_t)x + 1) * kept / (2 * fitted))];
    }
}

static int luma(const uint8_t rgb[768], uint8_t c) { return (rgb[3 * c] * 30 + rgb[3 * c + 1] * 59 + rgb[3 * c + 2] * 11) / 100; }

static int column_dark(const uint8_t *pixels, int w, int h, int x, const uint8_t rgb[768])
{
    for (int y = 0; y < h; y++) if (luma(rgb, pixels[(size_t)y * (size_t)w + (size_t)x]) > TGV_EDGE_DARK_LUMA) return 0;
    return 1;
}

void tgv_edges_add(TgvEdges *edges, const uint8_t *pixels, int w, int h, const uint8_t rgb[768])
{
    if (edges == NULL || pixels == NULL || rgb == NULL || w < 4 * TGV_WIDE_MAX_TRIM || h <= 0) return;
    int lit = 0;
    for (int y = 0; y < h && !lit; y += 4)
        for (int x = w / 4; x < 3 * w / 4; x += 4) if (luma(rgb, pixels[(size_t)y * (size_t)w + (size_t)x]) > TGV_EDGE_LIT_LUMA) { lit = 1; break; }
    if (!lit) return;
    edges->lit++;
    for (int k = 0; k < TGV_WIDE_MAX_TRIM; k++) {
        edges->dark_left[k] += column_dark(pixels, w, h, k, rgb);
        edges->dark_right[k] += column_dark(pixels, w, h, w - 1 - k, rgb);
    }
}

static int dark_run(const int dark[TGV_WIDE_MAX_TRIM], int lit)
{
    int n = 0;
    while (n < TGV_WIDE_MAX_TRIM && dark[n] * 2 > lit) n++;
    return n;
}

void tgv_edges_trim(const TgvEdges *edges, int *left, int *right)
{
    *left = *right = 0;
    if (edges == NULL || edges->lit == 0) return;
    *left = dark_run(edges->dark_left, edges->lit);
    *right = dark_run(edges->dark_right, edges->lit);
}
