/* See content_backdrop.h. */
#include "game/frontend/content_backdrop.h"

#include "game/sys/palette.h"

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
