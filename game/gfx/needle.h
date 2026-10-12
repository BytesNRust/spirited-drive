/* The tapered anti-aliased line of the gauges' needles (0048383b and its
 * leaves): the original keeps its state in the globals 0052d354..0052d3a4,
 * here a NeedleLine. Every coordinate is 16.16. The line runs along its
 * major axis (y when |dy| > |dx|, else x) between two edges, a and b, each
 * an offset on the minor axis with its own slope, so the needle widens
 * from its start width to its end width. */
#ifndef TNFS_GFX_NEEDLE_H
#define TNFS_GFX_NEEDLE_H

#include "game/gfx/quad.h"

#include <stdint.h>

typedef struct {
    int32_t right;          /* 0052d354: the clip, width << 16 (0041f818) */
    int32_t dx;             /* 0052d358 */
    int32_t bottom;         /* 0052d35c: height << 16 */
    int32_t dy;             /* 0052d360 */
    int32_t x_start;        /* 0052d364: x-major, the major axis from */
    int32_t clipped;        /* 0052d368: an edge left the clip, the edge pass runs first */
    int32_t xb;             /* 0052d36c: y-major, edge b's x */
    int32_t yb;             /* 0052d370: x-major, edge b's y */
    int32_t ya;             /* 0052d374: x-major, edge a's y */
    int32_t xa;             /* 0052d378: y-major, edge a's x */
    int32_t y_start;        /* 0052d37c: y-major, the major axis from */
    int32_t top;            /* 0052d380: the clip, 0 */
    int32_t left;           /* 0052d384: the clip, 0 */
    int32_t slope_xa;       /* 0052d388: y-major, edge a's x per y */
    int32_t slope_yb;       /* 0052d38c: x-major, edge b's y per x */
    int32_t slope_xb;       /* 0052d390: y-major, edge b's x per y */
    const uint8_t *fade;    /* 0052d394: the fade block (004f9744) past its first word */
    int32_t x_end;          /* 0052d398: x-major, the major axis to */
    int32_t y_end;          /* 0052d39c: y-major, the major axis to */
    int32_t slope_ya;       /* 0052d3a0: x-major, edge a's y per x */
    int32_t fade_first;     /* 0052d3a4: the fade block's first byte */
} NeedleLine;

/* 00482bad (watcall ()): the y-major line clipped to the clip rectangle.
 * 0 when it ends above the top or starts at or below the bottom. A start
 * above the top moves both edges down to it; the end is held to the
 * bottom. clipped = edge a left of the left or edge b at or right of the
 * right. Edge b left of the left: 0 unless its slope is positive, then the
 * start moves to where it crosses the left (0 when that start is at or
 * past `right`, as the original compares it, or above the left), edge b
 * held there and edge a moved along. Edge a at or right of the right: 0
 * unless its slope is negative, then the start moves to where it crosses
 * right - 1 (same tests), edge b moved along and edge a held there. 1
 * otherwise. */
int32_t needle_clip_y(NeedleLine *n);

/* 00482e15 (watcall ()): 00482bad with the axes swapped, the x-major line
 * clipped: x for y, left / right for top / bottom, edge a (ya) for xa and
 * edge b (yb) for xb. Here the new start after an edge crossing is tested
 * against right / left, its own axis. */
int32_t needle_clip_x(NeedleLine *n);

/* 004832c2 (watcall ()): the clipped y-major needle drawn row by row into
 * the 8-bit surface (004c78e0 + 004c78dc's row offsets: pixels + row *
 * pitch), from row y_start >> 16 while y_start < y_end, edge b left of
 * `right` and edge a not left of `left`. A row covers pixels xa >> 16 to
 * xb >> 16: the inner ones take fade_first, the outer two the fade row by
 * their cover in 16ths (edge a's (0x10000 - fraction) >> 12, 0 skipped;
 * edge b's fraction >> 12, 0 skipped), one pixel alone both covers - 16.
 * Then both edges move by their slopes and y_start by dy. Port guard: a
 * row whose edge b is left of edge a draws nothing (the original's count
 * runs down through 0 and on over memory). */
void needle_span_y(NeedleLine *n, uint8_t *pixels, int32_t pitch);

/* 0048369d (watcall ()): 004832c2 by columns for the x-major needle: from
 * column x_start >> 16 while x_start < x_end, edge b above `bottom` and
 * edge a not above `top`; pixels ya >> 16 down to yb >> 16, a step the
 * surface's width (004c78b4; the port's pitch, the same on a linear
 * surface); then the edges by their slopes and x_start by dx. Here one
 * pixel alone is left when its level (both covers - 16) is not above 0.
 * Port guard as 004832c2's for edge b above edge a. */
void needle_span_x(NeedleLine *n, uint8_t *pixels, int32_t pitch);

/* 0048307d (watcall ()): the y-major needle's rows that cross the
 * surface's clip (004c78bc..c8; left and right taken as their low 16 bits,
 * signed), through the surface's pixel read, plot and fill (0048f5f0,
 * 0049f3a0, 004829b0). Returns at the first row inside the clip (left <=
 * xa >> 16, xb >> 16 < right; 004832c2 takes over), or wholly left or
 * wholly right of it, or at y_end. A row: one pixel alone faded by both
 * covers - 16 when above 0; else edge a's pixel by its cover when that is
 * not 0 and the pixel is right of `left` (strictly: a pixel on the left
 * edge is left as it is), the pixels between filled with fade_first,
 * edge b's by its cover when not 0 and left of `right`. Then the edges by
 * their slopes, y_start by dy. */
void needle_edges_y(NeedleLine *n, const QuadTarget *t);

/* 00483458 (watcall ()): 0048307d by columns for the x-major needle: the
 * clip's top and bottom (low 16 bits, signed) for its left and right, a
 * column from ya >> 16 down to yb >> 16 (the fill 1 wide), the edges by
 * their slopes and x_start by dx. */
void needle_edges_x(NeedleLine *n, const QuadTarget *t);

#endif
