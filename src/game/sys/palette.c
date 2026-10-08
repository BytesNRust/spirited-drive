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
