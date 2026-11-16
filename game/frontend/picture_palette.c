/* See picture_palette.h. */
#include "game/frontend/picture_palette.h"

#include "game/sys/palette.h"

void picture_palette_nearest(const uint8_t *rgba, int n, const uint8_t palette[768], const uint8_t usable[256], uint8_t *out)
{
    int any = 0;
    for (int k = 0; k < 256 && !any; k++) any = usable[k] != 0;
    for (int i = 0; i < n; i++) {
        const uint8_t *px = rgba + 4 * i;
        out[i] = any ? (uint8_t)palette_nearest(palette, usable, px[0], px[1], px[2]) : 0;   /* the exe's own search (00473140) */
    }
}
