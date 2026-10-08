/* Polygon output of the track renderer: the recursive subdivision of near
 * quads (00425718) and the leaf that turns a cell of a texture into uv
 * corners (004255ec). The texture mapper itself is reached through function
 * pointers, as in the original (004f9e94 / 004f9e9c). */
#ifndef TNFS_TRACK_DRAW_H
#define TNFS_TRACK_DRAW_H

#include "formats/fsh.h"
#include "game/race/view.h"

#include <stdint.h>

typedef struct {
    int32_t x0, y0, x1, y1;  /* 004c78bc..004c78c8; x1, y1 exclusive */
} ClipRect;

typedef struct {
    void *user;
    /* 004f9e94: a cell of the texture, uv corners in 16.16 */
    void (*draw_uv)(void *user, const FshEntry *texture, const int32_t uv[8], const int32_t screen[8]);
    /* 004f9e9c: the whole texture */
    void (*draw_full)(void *user, const FshEntry *texture, const int32_t screen[8]);
} QuadSink;

typedef struct {
    const View *view;
    ClipRect clip;
    int world_shift;          /* 004c52bc */
    int32_t near_dir[3];      /* 00512040 / 00512044 / 00512048 (004497d8) */
    QuadSink sink;
} DrawContext;

/* 00487b0e: the outcodes (1 left, 2 right, 4 above, 8 below) shared by all
 * points; 0 when the polygon may be visible. */
unsigned outcode_shared(const ClipRect *clip, const int32_t *screen, int count);

/* 00425718. cam: camera-space corners; screen: their projections. u_level and
 * depth split the texture into cells (u_cell, v_cell); near is the near plane
 * of the 3D code (view struct +4). */
void track_draw_subdivided(const DrawContext *ctx, const FshEntry *texture, int u_level, int depth, int u_cell,
                           int v_cell, const int32_t *const cam[4], const int32_t screen[8], int32_t near);

#endif
