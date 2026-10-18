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

/* 004c5ff8: the colours the nearest-colour searches may pick, the menus' and
 * the race's. The image's table (all but 0xfe and 0xff: the pointer's and the
 * see-through one) until 0041f818 writes the race's at a race start; nothing
 * puts the image's back, so the menus after a race search the race's. */
const uint8_t *palette_usable(void);
void palette_usable_set(const uint8_t usable[PALETTE_COLORS]);

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

/* 0049a850: count 6-bit colours (0..63 a channel) to 8 bits, (v * 255 + 32)
 * / 63 a channel, kept to a byte (a channel above 63 wraps); channel by
 * channel, so src and dst may be the same. */
void palette_6_to_8(int count, const uint8_t *src, uint8_t *dst);

/* 0048f344: each of count pixels becomes table[pixel], in place, the table
 * read as it stands at each step (the original's callers keep it apart). */
void palette_remap_pixels(uint8_t *pixels, const uint8_t *table, uint32_t count);

/* Brightness of frame n (0..PALETTE_FADE_STEPS-1) of a fade. */
int palette_fade_in_step(int frame);
int palette_fade_out_step(int frame);

#endif
