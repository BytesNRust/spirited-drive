/* The tyre smoke's puffs (00456820). */
#include "game/race/puffs.h"

#include "game/gfx/sprite_scaled.h"

enum { CHANNEL_MAX = 0xff, DIRT_MOST = 0x10000, SIX_BIT_MAX = 0x3f, SPARE = 0x80 };

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

void puff_blend_init(PuffBlend *b, uint8_t table[PUFF_BLEND_SIZE], const RacePalette *p,
                     uint8_t (*nearest)(void *ctx, uint32_t rgb), void *ctx)
{
    uint8_t six[3 * PUFF_BLEND_ROWS];
    race_palette_read(p, 0, PUFF_BLEND_ROWS, six);
    for (int i = 0; i < PUFF_BLEND_ROWS; i++) {
        for (int c = 0; c < 3; c++)
            b->palette[4 * i + 2 - c] = (uint8_t)(six[3 * i + c] * CHANNEL_MAX / SIX_BIT_MAX);
        b->palette[4 * i + 3] = SPARE;
    }
    b->palette[sizeof b->palette - 1] = 0;
    puff_blend_build(table, b->palette, nearest, ctx);
    for (int a = 0; a < PUFF_BLEND_ROWS; a++)
        b->rows[a] = table + a * 256;
}

static int32_t at_resolution(const PuffView *v, int32_t size)
{
    return v->high_res != 0 ? size : size / 2;
}

void puff_smoke(const PuffView *v, int32_t x, int32_t y, int32_t size)
{
    const int32_t pick = v->rand_below(v->ctx, 2) * 2;
    sprite_scaled_blend_at(v->target, v->blend, v->smoke[pick], at_resolution(v, size), x, y);
}

void puff_dirt(const PuffView *v, int32_t x, int32_t y, int32_t size)
{
    const int32_t pick = v->rand_below(v->ctx, 3);
    size = at_resolution(v, size);
    if (size > DIRT_MOST) size = DIRT_MOST;
    sprite_scaled_at(v->target, v->dirt[pick], size, x, y);
}
