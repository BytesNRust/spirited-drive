/* See content_backdrop.h. */
#include "game/frontend/content_backdrop.h"

#include "game/sys/palette.h"

#include <stddef.h>

void content_backdrop_usable(uint8_t usable[256])
{
    for (int i = 0; i < 256; i++) usable[i] = i != CONTENT_BACKDROP_POINTER && i != CONTENT_BACKDROP_CLEAR;
}

void content_backdrop_darken(const uint8_t pal[768], uint32_t rgb, int num, int den, uint8_t table[256])
{
    uint8_t usable[256];
    content_backdrop_usable(usable);
    const int toward[3] = { (int)(rgb >> 16 & 0xff), (int)(rgb >> 8 & 0xff), (int)(rgb & 0xff) };
    for (int i = 0; i < 256; i++) {
        int c[3];
        for (int k = 0; k < 3; k++) c[k] = (pal[3 * i + k] * (den - num) + toward[k] * num + den / 2) / den;
        table[i] = usable[i] ? (uint8_t)palette_nearest(pal, usable, c[0], c[1], c[2]) : (uint8_t)i;
    }
}

int content_backdrop_nearest(const uint8_t pal[768], uint32_t rgb)
{
    uint8_t usable[256];
    content_backdrop_usable(usable);
    return palette_nearest(pal, usable, (int)(rgb >> 16 & 0xff), (int)(rgb >> 8 & 0xff), (int)(rgb & 0xff));
}

void content_backdrop_panel(uint8_t *pixels, int w, int h, const uint8_t table[256], int x0, int y0, int x1, int y1, uint8_t edge)
{
    for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < h; y++)
        for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < w; x++) {
            uint8_t *const p = pixels + (size_t)y * (size_t)w + (size_t)x;
            *p = x == x0 || x == x1 || y == y0 || y == y1 ? edge : table[*p];
        }
}
