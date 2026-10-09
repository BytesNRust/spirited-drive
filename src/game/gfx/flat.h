/* EA's flat-colour polygon and line fill (NFS_WIN.EXE 0048434e, 00484377,
 * 00484389 and 00484254), used by the race for 2D overlays and, through
 * 004a3390, for 3D objects drawn with one colour per polygon.
 *
 * Points are int32 pixel coordinates, (x, y) per point. The clip rectangle is
 * the game's: left and top inclusive, right and bottom exclusive (004c78bc,
 * 004c78c0, 004c78c4, 004c78c8), carried by QuadTarget like the quad mapper.
 * Only the low byte of a colour is drawn: these are 8-bit surfaces.
 *
 * The scan converter keeps one left and one right x per row in tables of 600
 * rows, so clip_bottom must be at most 600; an invalid target draws nothing.
 *
 * Quirks of the original that are kept: a polygon is not drawn when its
 * leftmost x is the last visible column (clip_right - 1), and its bottom row
 * is drawn (a polygon from y 10 to y 12 fills rows 10, 11 and 12). A polygon
 * with all points on one column is drawn as a line, one with all points on one
 * row is not drawn at all.
 *
 * TODO(004a3390): the dispatcher that picks flat_line / flat_polygon by corner
 *                 count for 3D objects (object3d.c carries the call sites).
 * TODO(004a02d8 and neighbours): 16-bit and 32-bit destination surfaces. */
#ifndef TNFS_FLAT_H
#define TNFS_FLAT_H

#include "game/gfx/quad.h"

#include <stdint.h>

enum { FLAT_MAX_ROWS = 600 };

/* 0048434e (points through a pointer) and 00484389 (the same points passed
 * inline on the stack, which in C is the same call): fills the polygon through
 * `count` points in the given order. One point draws a one-pixel line. The
 * polygon should be convex; a concave one leaves rows the outline never
 * reaches at 0 (the original leaves whatever the stack held). Where the two
 * edges of a row cross, the original takes the whole run of the first edge
 * that falls outside the other, which is right for convex input only. */
void flat_polygon(const QuadTarget *target, uint32_t colour, int count, const int32_t *points);

/* 00484377: the same, but the second edge chain is merged into each row by
 * comparing every row on its own (a per-row min and max) instead of the
 * convex shortcut, so concave and self-crossing outlines draw their per-row
 * extent. Only the merge differs: the first chain, the clipping and the span
 * filler are shared. */
void flat_polygon_merge(const QuadTarget *target, uint32_t colour, int count,
                        const int32_t *points);

/* 00484254: a line from (x0, y0) to (x1, y1), both ends drawn. */
void flat_line(const QuadTarget *target, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
               uint32_t colour);

#endif
