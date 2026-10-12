/* The tapered anti-aliased line of the gauges' needles (0048383b and its
 * leaves): the original keeps its state in the globals 0052d354..0052d3a4,
 * here a NeedleLine. Every coordinate is 16.16. The line runs along its
 * major axis (y when |dy| > |dx|, else x) between two edges, a and b, each
 * an offset on the minor axis with its own slope, so the needle widens
 * from its start width to its end width. */
#ifndef TNFS_GFX_NEEDLE_H
#define TNFS_GFX_NEEDLE_H

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

#endif
