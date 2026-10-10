/* Palette arithmetic behind the fades of EA's system layer. */
#ifndef TNFS_PALETTE_H
#define TNFS_PALETTE_H

#include <stdint.h>

enum {
    PALETTE_COLORS = 256,
    PALETTE_BYTES = 768,
    PALETTE_FULL_STEP = 32,
    PALETTE_FADE_STEPS = 17
};

/* 004920bc: dst[0..count*3) = src[first*3..] * step >> 5. */
void palette_scale(const uint8_t *src, uint8_t *dst, int first, int count, int step);

/* 00473140 (8-bit path): index of the usable colour with the smallest squared
 * RGB distance; ties keep the lowest index. */
int palette_nearest(const uint8_t *pal, const uint8_t usable[256], int r, int g, int b);

/* 0048f370: 16 palette indices shading from colour bg (level 0) towards fg,
 * (fg * i + bg * (15 - i)) >> 4 per channel. Text anti-aliasing uses them. */
void palette_text_ramp(const uint8_t *pal, const uint8_t usable[256], int fg, int bg, uint8_t ramp[16]);

/* 0048263c(k, k, k, pal, table) / 004824a0: table[i] = colour closest to
 * palette[i] scaled by k / 65536 (rounded, clamped to 63), weighted
 * 2*dr^2 + 3*dg^2 + db^2. Dialog boxes darken what is behind them with it. */
void palette_shade_table(const uint8_t *pal, const uint8_t usable[256], int k, uint8_t table[256]);

/* Brightness of frame n (0..PALETTE_FADE_STEPS-1) of a fade. */
int palette_fade_in_step(int frame);
int palette_fade_out_step(int frame);

#endif
