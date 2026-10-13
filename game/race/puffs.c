/* The tyre smoke's puffs (00456820). */
#include "game/race/puffs.h"

enum { CHANNEL_MAX = 0xff };

static int32_t lift(int32_t under, int32_t added)
{
    const int32_t c = under + added;
    return c > CHANNEL_MAX ? CHANNEL_MAX : c;
}

void puff_blend_build(uint8_t table[PUFF_BLEND_SIZE], const uint8_t palette[1024],
                      uint8_t (*nearest)(void *ctx, uint32_t rgb), void *ctx)
{
    for (int a = 0; a < 256; a++) {
        const uint8_t *top = palette + 4 * a;
        const int32_t add0 = top[0] - top[0] / 4, add1 = top[1] - top[1] / 4, add2 = top[2] - top[2] / 4;
        for (int b = 0; b < 256; b++) {
            const uint8_t *under = palette + 4 * b;
            const uint32_t rgb = (uint32_t)lift(under[0], add0) | (uint32_t)lift(under[1], add1) << 8 |
                                 (uint32_t)lift(under[2], add2) << 16;
            table[a * 256 + b] = nearest(ctx, rgb);
        }
    }
}
