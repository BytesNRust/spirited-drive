/* See content_preview.h. */
#include "game/frontend/content_preview.h"

#include <stddef.h>

/* how much of source cell [cell, cell + 1) lies within [lo, hi) */
static double overlap(int cell, double lo, double hi)
{
    const double a = cell > lo ? cell : lo, b = cell + 1 < hi ? cell + 1 : hi;
    return b > a ? b - a : 0.0;
}

/* output pixel (ox, oy): the source rectangle it stands for, its cells weighted by the part of each it covers */
static void reduce_one(const uint8_t *rgb, int w, int h, uint8_t *out, int ox, int oy, double sx, double sy)
{
    const double x0 = ox * sx, x1 = (ox + 1) * sx, y0 = oy * sy, y1 = (oy + 1) * sy;
    double sum[3] = { 0.0, 0.0, 0.0 }, area = 0.0;
    for (int y = (int)y0; y < h && y < y1; y++) {
        const double wy = overlap(y, y0, y1);
        for (int x = (int)x0; x < w && x < x1; x++) {
            const double wxy = wy * overlap(x, x0, x1);
            const uint8_t *p = rgb + 3 * ((long)y * w + x);
            for (int c = 0; c < 3; c++) sum[c] += wxy * p[c];
            area += wxy;
        }
    }
    for (int c = 0; c < 3; c++) {
        const double v = area > 0.0 ? sum[c] / area + 0.5 : 0.0;
        out[c] = (uint8_t)(v > 255.0 ? 255 : (int)v);
    }
}

void content_reduce_area(const uint8_t *rgb, int w, int h, uint8_t *out_rgb, int ow, int oh)
{
    if (rgb == NULL || out_rgb == NULL || w <= 0 || h <= 0 || ow <= 0 || oh <= 0) return;
    const double sx = (double)w / ow, sy = (double)h / oh;
    for (int oy = 0; oy < oh; oy++)
        for (int ox = 0; ox < ow; ox++) reduce_one(rgb, w, h, out_rgb + 3 * ((long)oy * ow + ox), ox, oy, sx, sy);
}
