/* See picture_palette.h. */
#include "game/frontend/picture_palette.h"

#include "game/sys/palette.h"

#include <stdlib.h>
#include <string.h>

void picture_palette_nearest(const uint8_t *rgba, int n, const uint8_t palette[768], const uint8_t usable[256], uint8_t *out)
{
    int any = 0;
    for (int k = 0; k < 256 && !any; k++) any = usable[k] != 0;
    for (int i = 0; i < n; i++) {
        const uint8_t *px = rgba + 4 * i;
        out[i] = any ? (uint8_t)palette_nearest(palette, usable, px[0], px[1], px[2]) : 0;   /* the exe's own search (00473140) */
    }
}

static int clamp_channel(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

/* the usable entry nearest (r, g, b) by squared distance, the first of equals; unlike the exe's search no distance is too far, so a usable
 * entry always comes back */
static int usable_nearest(const uint8_t palette[768], const uint8_t usable[256], const int c[3])
{
    int best = -1, best_distance = 0;
    for (int i = 0; i < 256; i++) {
        if (!usable[i]) continue;
        const int dr = c[0] - palette[3 * i], dg = c[1] - palette[3 * i + 1], db = c[2] - palette[3 * i + 2];
        const int distance = dr * dr + dg * dg + db * db;
        if (best < 0 || distance < best_distance) best = i, best_distance = distance;
    }
    return best;
}

/* the error (in sixteenths, as Floyd-Steinberg spreads it: 7 right, 3 below left, 5 below, 1 below right) of this row and the next */
void picture_palette_dither(const uint8_t *rgba, int w, int h, const uint8_t palette[768], const uint8_t usable[256], uint8_t *out)
{
    int any = 0;
    for (int k = 0; k < 256 && !any; k++) any = usable[k] != 0;
    int *err = any && w > 0 ? calloc((size_t)(w + 2) * 2 * 3, sizeof *err) : NULL;
    if (err == NULL) {   /* nothing usable (or no memory): as picture_palette_nearest does */
        picture_palette_nearest(rgba, w * h, palette, usable, out);
        return;
    }
    for (int y = 0; y < h; y++) {
        int *now = err + (size_t)(y & 1) * (w + 2) * 3, *next = err + (size_t)((y + 1) & 1) * (w + 2) * 3;
        memset(next, 0, (size_t)(w + 2) * 3 * sizeof *next);
        for (int x = 0; x < w; x++) {
            const uint8_t *px = rgba + 4 * ((size_t)y * w + x);
            int c[3];
            for (int k = 0; k < 3; k++) c[k] = clamp_channel(px[k] + (now[3 * (x + 1) + k] + 8 * (now[3 * (x + 1) + k] > 0 ? 1 : -1)) / 16);
            const int i = usable_nearest(palette, usable, c);
            out[(size_t)y * w + x] = (uint8_t)i;
            for (int k = 0; k < 3; k++) {
                const int e = c[k] - palette[3 * i + k];
                now[3 * (x + 2) + k] += 7 * e, next[3 * x + k] += 3 * e, next[3 * (x + 1) + k] += 5 * e, next[3 * (x + 2) + k] += e;
            }
        }
    }
    free(err);
}
