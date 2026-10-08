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

/* Brightness of frame n (0..PALETTE_FADE_STEPS-1) of a fade. */
int palette_fade_in_step(int frame);
int palette_fade_out_step(int frame);

#endif
