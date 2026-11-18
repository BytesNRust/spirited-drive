/* See gallery_photo.h. */
#include "game/frontend/gallery_photo.h"

#include <string.h>

int gallery_cover(const uint8_t *src, int sw, int sh, uint8_t *dst, int dw, int dh)
{
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return 0;
    /* the factor in 1/1024ths: source pixels per destination pixel, the smaller of the two axes' so that the source covers dst and is cut on
     * the other axis */
    const long fx = (long)sw * 1024 / dw, fy = (long)sh * 1024 / dh, f = fx < fy ? fx : fy;
    const long used_w = f * dw, used_h = f * dh;   /* the source's part used, in 1/1024ths */
    const long x_off = ((long)sw * 1024 - used_w) / 2, y_off = ((long)sh * 1024 - used_h) / 2;
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++) {
            long x0 = (x_off + f * x) / 1024, x1 = (x_off + f * (x + 1) + 1023) / 1024;
            long y0 = (y_off + f * y) / 1024, y1 = (y_off + f * (y + 1) + 1023) / 1024;
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            if (x1 > sw) x1 = sw;
            if (y1 > sh) y1 = sh;
            if (x0 >= sw) x0 = sw - 1;
            if (y0 >= sh) y0 = sh - 1;
            long sum[3] = { 0, 0, 0 }, n = 0;
            for (long yy = y0; yy < y1; yy++)
                for (long xx = x0; xx < x1; xx++, n++)
                    for (int k = 0; k < 3; k++) sum[k] += src[3 * (yy * sw + xx) + k];
            for (int k = 0; k < 3; k++) dst[3 * ((size_t)y * dw + x) + k] = (uint8_t)(sum[k] / n);
        }
    return 1;
}

int gallery_placeholder(const uint8_t *ground, int gw, int gh, uint8_t *dst, int dw, int dh)
{
    if (dw <= 0 || dh <= 0 || gw < dw || gh < dh) return 0;
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++) {
            const int edge = x == 6 || y == 6 || x == dw - 7 || y == dh - 7;   /* a lighter line inside, as a mount's */
            const int inside = x >= 6 && y >= 6 && x <= dw - 7 && y <= dh - 7;
            for (int k = 0; k < 3; k++) {
                const int g = ground[3 * ((size_t)y * gw + x) + k];
                dst[3 * ((size_t)y * dw + x) + k] = (uint8_t)(edge && inside ? g * 3 / 4 + 40 : g * 2 / 5);
            }
        }
    return 1;
}
