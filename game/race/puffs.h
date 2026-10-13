/* The tyre smoke's puffs (00456948, 00456820): the additive blend their
 * sprites draw through. */
#ifndef TNFS_PUFFS_H
#define TNFS_PUFFS_H

#include "game/gfx/quad.h"

#include <stdint.h>

enum { PUFF_BLEND_SIZE = 256 * 256 };

/* 00456820 (watcall (table, palette)): palette 256 x 4 bytes (three channels
 * 0..255 then a spare). table[a x 256 + b] = nearest(min(pal[b] + pal[a] -
 * pal[a] / 4, 255) per channel), the channels packed c0 | c1 << 8 | c2 << 16
 * (00473140). Row a is the sprite's texel, column b what lies under it. */
void puff_blend_build(uint8_t table[PUFF_BLEND_SIZE], const uint8_t palette[1024],
                      uint8_t (*nearest)(void *ctx, uint32_t rgb), void *ctx);

enum { PUFF_SHAPES = 4 };

typedef struct {
    const QuadTarget *target;
    const uint8_t *const *blend;              /* *004c795c: 256 rows */
    const uint8_t *smoke[PUFF_SHAPES];        /* 00513e88: 'smo1'..'smo4' */
    const uint8_t *dirt[PUFF_SHAPES];         /* 00514298: 'dir1'..'dir4' */
    int32_t high_res;                         /* 004fa0c0 */
    void *ctx;
    int32_t (*rand_below)(void *ctx, int32_t n);  /* 00443730 */
} PuffView;

/* 00456bd4 (stdcall x, y, size): smoke[2 x rand(2)] at (x, y), scaled by
 * size (halved, toward zero, at low resolution), blended (00490570). */
void puff_smoke(const PuffView *v, int32_t x, int32_t y, int32_t size);

/* 00456c28 (stdcall x, y, size): dirt[rand(3)], size halved likewise then at
 * most 0x10000, colour 0xff left out (00490b00). */
void puff_dirt(const PuffView *v, int32_t x, int32_t y, int32_t size);

#endif
