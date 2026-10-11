#include "game/sys/palette.h"

void palette_scale(const uint8_t *src, uint8_t *dst, int first, int count, int step)
{
    const uint8_t *p = src + first * 3;
    for (int i = 0; i < count * 3; i++) dst[i] = (uint8_t)((p[i] * step) >> 5);
}

/* 00463e7c counts 0, 2, ..., 32; 00463ef0 counts 32, 30, ..., 0. */
int palette_fade_in_step(int frame)
{
    return frame * 2;
}

int palette_fade_out_step(int frame)
{
    return PALETTE_FULL_STEP - frame * 2;
}

int palette_nearest(const uint8_t *pal, const uint8_t usable[256], int r, int g, int b)
{
    int best = 0, best_distance = 99999;  /* 00473140 starts from 99999, strict < */
    for (int i = 0; i < PALETTE_COLORS; i++) {
        if (!usable[i]) continue;
        const int dr = r - pal[3 * i], dg = g - pal[3 * i + 1], db = b - pal[3 * i + 2];
        const int distance = dr * dr + dg * dg + db * db;
        if (distance < best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    return best;
}

void palette_text_ramp(const uint8_t *pal, const uint8_t usable[256], int fg, int bg, uint8_t ramp[16])
{
    const uint8_t *f = pal + 3 * fg, *k = pal + 3 * bg;
    for (int level = 0; level < 16; level++) {
        int c[3];
        for (int ch = 0; ch < 3; ch++) c[ch] = (f[ch] * level + k[ch] * (15 - level)) >> 4;
        ramp[level] = (uint8_t)palette_nearest(pal, usable, c[0], c[1], c[2]);
    }
}

/* Fixed-point scale rounded half up, as 004824a0: (c * k) >> 16 plus bit 15. */
static int shade_channel(int c, int k)
{
    const long product = (long)c * k;
    const int v = (int)(product >> 16) + (int)((product >> 15) & 1);
    return v > 0x3F ? 0x3F : v;
}

static int weighted_nearest(const uint8_t *pal, const uint8_t usable[256], int r, int g, int b)
{
    int best = 0, best_distance = 9999;
    for (int i = 0; i < PALETTE_COLORS; i++) {
        if (!usable[i]) continue;
        const int dr = r - pal[3 * i], dg = g - pal[3 * i + 1], db = b - pal[3 * i + 2];
        const int distance = 2 * dr * dr + 3 * dg * dg + db * db;
        if (distance < best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    return best;
}

void palette_shade_table(const uint8_t *pal, const uint8_t usable[256], int k, uint8_t table[256])
{
    for (int i = 0; i < PALETTE_COLORS; i++) {
        const uint8_t *c = pal + 3 * i;
        table[i] = (uint8_t)weighted_nearest(pal, usable, shade_channel(c[0], k), shade_channel(c[1], k),
                                             shade_channel(c[2], k));
    }
}

void palette_6_to_8(int count, const uint8_t *src, uint8_t *dst)
{
    for (int i = 0; i < count; i++, src += 3, dst += 3)
        for (int ch = 0; ch < 3; ch++)
            dst[ch] = (uint8_t)((src[ch] * 0xff + 0x20) / 0x3f);
}

void palette_remap_pixels(uint8_t *pixels, const uint8_t *table, uint32_t count)
{
    for (; count != 0; count--, pixels++)
        *pixels = table[*pixels];
}
