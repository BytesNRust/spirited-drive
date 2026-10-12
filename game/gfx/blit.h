/* The 2D shape draw 00496510 (an FSH shape at x, y, clipped to the target's
 * clip rectangle), for an 8-bit shape (type 0x7b) on an 8-bit surface: each
 * row through the caller's row table, 004c61b8 (a straight copy, 0048ec60)
 * or 004c61d4 (colour 0xff skipped). A shape of another type is converted
 * first in the original (00496fd0 / 004970a4); the port does not draw it.
 * The original's banked-surface split and its time slices (004c78e8) are
 * left out: the port's surfaces are linear. */
#ifndef TNFS_BLIT_H
#define TNFS_BLIT_H

#include "game/gfx/quad.h"

#include <stdint.h>

typedef enum {
    BLIT_OPAQUE = 0,        /* 004c61b8 */
    BLIT_TRANSPARENT = 1    /* 004c61d4 */
} BlitMode;

/* 00496510: returns 0, or -1 for a shape the port does not draw. */
int blit_shape(const QuadTarget *t, const uint8_t *shape, int32_t x, int32_t y, BlitMode mode);

/* 00476e28: the surface under the shape's box at (x, y), clipped as the
 * draw, copied into the shape's pixels (00496510 with the row table 004c6260);
 * then the shape's place (+0xc, +0xe) = x, y, also when nothing was copied. */
int blit_grab(const QuadTarget *t, uint8_t *shape, int32_t x, int32_t y);

/* 00473780: the shape at its own place (+0xc, +0xe), opaque. */
int blit_shape_placed(const QuadTarget *t, const uint8_t *shape);

/* 004737d0: the shape at (x, y), opaque. */
int blit_shape_at(const QuadTarget *t, const uint8_t *shape, int32_t x, int32_t y);

/* 0047380c: the shape opaque with its hotspot (the words at +8, +0xa) at
 * (x, y). */
int blit_shape_centred(const QuadTarget *t, const uint8_t *shape, int32_t x, int32_t y);

/* 00473ab4: the shape at its own place, colour 0xff skipped (004739b0 with
 * 004c61d4; its 16-bit surface path is not ported). */
int blit_shape_placed_transparent(const QuadTarget *t, const uint8_t *shape);

/* 00473a98: the shape at (x, y), colour 0xff skipped (004739b0). */
int blit_shape_at_transparent(const QuadTarget *t, const uint8_t *shape, int32_t x, int32_t y);

/* 00483fa0: the runs of a run list copied from an 8-bit shape (type 0x7b) of
 * the screen's size to the surface, `width` pixels a row (004c78b4). The list
 * (an .mmb part): +8 a word, the 64k-pixel blocks; +0xa a word, the entries of
 * the index table 0049f580 builds; +0xc the 16.16 step through the source;
 * from +0x14, for each block a word (its runs) and that many runs of 8 bytes:
 * the source pixel (a long, divided by the shape's width into row and column),
 * the destination pixel in the block (a word) and the count (a word). A run
 * past the end of a row goes on at column 0 of the next source and
 * destination rows. Each pixel is source[(k x step) >> 16] (0048cc49 through
 * the index table); a run longer than the table reads the table's stale
 * entries in the original, which the port does not keep. Left out as in
 * blit_shape: the banked surface (004c78d8), the time slices (004c6184,
 * 00477470) and the flag 004c6180 that skips the whole draw. Returns 0, or -1
 * for a shape the port does not draw. Where the original divides by zero (a
 * shape or a surface of width 0) the port stops. */
int blit_runs(const QuadTarget *t, int32_t width, const uint8_t *shape, const uint8_t *runs);

#endif
