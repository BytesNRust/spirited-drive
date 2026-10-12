/* Shared pieces of the flat-colour polygon and line code (flat*.c). */
#ifndef TNFS_FLAT_INTERNAL_H
#define TNFS_FLAT_INTERNAL_H

#include "game/gfx/flat.h"

#include <stdint.h>

/* Octant of an edge (byte 0x34 of the original's edge record). An edge always
 * runs downwards (y0 <= y1); "left" and "right" give the x direction. */
enum {
    EDGE_HORIZONTAL_LEFT = 0,   /* x0 > x1 on one row, stored swapped */
    EDGE_HORIZONTAL = 1,
    EDGE_VERTICAL = 2,
    EDGE_DIAGONAL_LEFT = 3,     /* |dx| == dy, x falls */
    EDGE_DIAGONAL_RIGHT = 4,
    EDGE_STEEP_LEFT = 5,        /* |dx| < dy: one step per row, x falls */
    EDGE_STEEP_RIGHT = 6,
    EDGE_SHALLOW_LEFT = 7,      /* |dx| > dy: one step per column, x falls */
    EDGE_SHALLOW_RIGHT = 8,
    EDGE_POINT = 9,
    EDGE_UNSET = 0xFF
};

/* The edge record 0049f73f / 0049f730 build and the scan converters consume
 * (0x38 bytes on the original's stack). x and y are 32.32 fixed point: the
 * integer part in x / y, the fraction in x_frac / y_frac. */
typedef struct {
    uint32_t x_frac;       /* +00 */
    int32_t x;             /* +04 */
    uint32_t y_frac;       /* +08 */
    int32_t y;             /* +0c */
    int32_t x_end;         /* +10 */
    int32_t y_end;         /* +14 */
    int32_t count;         /* +18 steps to draw; 0 when nothing is left */
    int32_t colour;        /* +1c lines only */
    int32_t left_before;   /* +20 rows above the edge where the outline is left of the clip */
    int32_t left_after;    /* +24 rows below the edge, left of the clip */
    int32_t right_before;  /* +28 rows above the edge, right of the clip */
    int32_t right_after;   /* +2c rows below the edge, right of the clip */
    uint32_t slope;        /* +30 minor / major step as 0.32 */
    uint8_t kind;          /* +34 EDGE_* */
    uint8_t outcode;       /* +35 why an edge was rejected: 1 right, 2 left, 4 above, 8 below */
} Edge;

typedef struct {
    int32_t left, top, right, bottom;
} Clip;

/* 0049f73f (clipping) and 0049f730 (no clipping, the edge is known to lie
 * inside the clip rectangle): sets up the edge from (x0, y0) to (x1, y1).
 * Returns 0 when the edge is usable (it may still have count 0) or the
 * outcode when it lies wholly outside. */
int flat_edge_setup(const Clip *clip, int clipping, int32_t x0, int32_t y0, int32_t x1,
                    int32_t y1, Edge *edge);

/* Per-row left (min) and right (max) x of an outline, indexed by row. The
 * original keeps both on the stack, one after the other, so a write past the
 * left table lands in the right one; the same layout is kept, with writes
 * outside both dropped. */
enum { TABLE_WORDS = 2 * FLAT_MAX_ROWS + 8 };

typedef struct {
    int32_t words[TABLE_WORDS];
} RowTables;

static inline int32_t table_get(const RowTables *t, int64_t i)
{
    return i >= 0 && i < TABLE_WORDS ? t->words[i] : 0;
}

static inline void table_set(RowTables *t, int64_t i, int32_t v)
{
    if (i >= 0 && i < TABLE_WORDS) t->words[i] = v;
}

/* 00484596 / 00484654 and 00484786: scan an edge into the tables. The first
 * chain writes both tables; the second merges into them, by the convex
 * shortcut (merge == 0, flag byte 0) or row by row (merge == 1). */
void flat_scan_first(RowTables *tables, const Clip *clip, int clipping, const Edge *edge);
void flat_scan_second(RowTables *tables, const Clip *clip, int clipping, int merge,
                      const Edge *edge);

/* 0049f3a0 for an 8-bit surface: one clipped pixel (flat_fill_rect is in
 * flat.h). */
void flat_plot(const QuadTarget *target, int32_t x, int32_t y, uint32_t colour);

/* 004a0048: draws a set-up edge as a line of edge->colour. */
void flat_draw_edge(const QuadTarget *target, const Edge *edge);

#endif
