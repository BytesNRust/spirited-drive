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

/* 00473780: the shape at its own place (+0xc, +0xe), opaque. */
int blit_shape_placed(const QuadTarget *t, const uint8_t *shape);

#endif
