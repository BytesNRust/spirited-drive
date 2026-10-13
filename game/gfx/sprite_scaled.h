/* A shape drawn scaled and blended (004900a8 through 0048cae9), as the tyre
 * smoke's puffs are. */
#ifndef TNFS_SPRITE_SCALED_H
#define TNFS_SPRITE_SCALED_H

#include "game/gfx/quad.h"

#include <stdint.h>

/* 004900a8 (cdecl w, h, shape, x, y) for an 8-bit shape (type 0x7b) on the
 * 8-bit surface: the shape (width word +4, height +6, pixels from +0x10, a
 * row each width bytes) stretched to |w| x |h| at (x, y), mirrored for a
 * negative w or h; nothing for w or h 0 or a box off the clip. du = (width
 * << 16) / w, dv = (height << 16) / h, each starting at half a step (plus
 * the whole width / height when mirrored); clipped at the top and left by
 * stepping on, at the bottom and right by cutting. Each pixel becomes
 * blend[texel][pixel] (0048cae9, *004c795c: 256 rows of 256). Returns 0, or -1
 * for a shape of another type (the original converts it first, 0049738c). */
int sprite_scaled_blend(const QuadTarget *t, const uint8_t *const blend[256], const uint8_t *shape, int32_t w, int32_t h,
                        int32_t x, int32_t y);

/* 00490638: the same, each texel but colour 0xff copied (0048c89a). */
int sprite_scaled(const QuadTarget *t, const uint8_t *shape, int32_t w, int32_t h, int32_t x, int32_t y);

/* 00490440 (cdecl scale, shape, x, y; scale 16.16): w = scale x width >> 16,
 * h = |scale| x height >> 16 (32-bit products), the hotspot (words +8, +0xa)
 * at (x, y): x - round(scale x hot x) (x + round(scale x (width - hot x))
 * when scale < 0, mirrored), y - round(|scale| x hot y), round = the 64-bit
 * product >> 16 plus its bit 15; then 004900a8. */
int sprite_scaled_blend_at(const QuadTarget *t, const uint8_t *const blend[256], const uint8_t *shape, int32_t scale,
                           int32_t x, int32_t y);

/* 004909d0: the same through 00490638. */
int sprite_scaled_at(const QuadTarget *t, const uint8_t *shape, int32_t scale, int32_t x, int32_t y);

#endif
