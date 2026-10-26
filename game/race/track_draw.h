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
    /* 004f9e90: the whole texture, the faces of two-sided scenery */
    void (*draw_full_alt)(void *user, const FshEntry *texture, const int32_t screen[8]);
    /* 00458c04: a 3D scenery object (FAM objects child `model`) at a world
     * position (16.16) with a 24-bit heading; the 3D engine is not ported */
    void (*draw_model)(void *user, int model, const int32_t position[3], uint32_t heading);
    /* 0045a848: the cars on a road node, between the rows; may be NULL */
    void (*draw_cars)(void *user, int32_t node);
    /* 00459eb8 (-999): the skid marks about road node `row`, kept to the side
     * of s -> t (the row's last polygon's corners 0 and 3), in the near pass
     * before its cars; may be NULL */
    void (*draw_skids)(void *user, int32_t row, const int32_t s[2], const int32_t t[2]);
    /* port (spike): a near road cell from its camera-space corners, uv 16.16 as draw_uv's; NULL the exe's path
     * (00425718's subdivision, the near-plane push, the affine mapper) */
    void (*draw_camera)(void *user, const View *view, const FshEntry *texture, const int32_t uv[8],
                        const int32_t *const cam[4]);
    /* port: the same for the scenery boards the exe drops for a point nearer than 0x86 (0044a4b0), whatever Modern
     * 3D's setting; NULL the exe's (no board) */
    void (*port_near_camera)(void *user, const View *view, const FshEntry *texture, const int32_t uv[8],
                             const int32_t *const cam[4]);
} QuadSink;

typedef struct {
    const View *view;
    ClipRect clip;
    int world_shift;          /* 004c52bc */
    int32_t near_dir[3];      /* 00512040 / 00512044 / 00512048 (004497d8) */
    QuadSink sink;
    int32_t near;             /* *(00511fe8) +4: 004c52c0 >> shift, as 0044aa94 leaves it */
    int32_t port_near_scale;  /* port: 16.16 on 004c52c0 for a field wider than the exe's (the road to the view's
                               * bottom); 0 the exe's */
} DrawContext;

/* port (the PS1 look's light): the renderer's glue calls this, when it is set, with the four corner intensities of the terrain
 * polygon the draw callbacks that follow paint (255 full, 0 black), and with NULL when that polygon is done. The sink
 * (ported_view.c) sets it; NULL, the default, no light. */
extern void (*track_light_hook)(const uint8_t corners[4]);
/* 1 while the polygon the hook was last given has all its corners in a tunnel's rows (the light's cast is for those only); 0 otherwise */
extern int track_light_tunnel;

/* 00487b0e: the outcodes (1 left, 2 right, 4 above, 8 below) shared by all
 * points; 0 when the polygon may be visible. */
unsigned outcode_shared(const ClipRect *clip, const int32_t *screen, int count);

/* 004255ec: cell (u_cell, v_cell) of a texture split 2^u_level by
 * 2^v_level, counted from the far edge, through draw_uv. */
void track_draw_cell(const DrawContext *ctx, const FshEntry *t, int u_level, int v_level, int u_cell, int v_cell,
                     const int32_t screen[8]);

/* A camera-space point projected; one in front of the near plane is first
 * pushed onto it along near_dir (as 00425718 / 00425ed4 do). */
void track_project_near(const DrawContext *ctx, const int32_t p[3], int32_t near, int32_t screen[2]);

/* 00425718. cam: camera-space corners; screen: their projections. u_level and
 * depth split the texture into cells (u_cell, v_cell); near is the near plane
 * of the 3D code (view struct +4). */
void track_draw_subdivided(const DrawContext *ctx, const FshEntry *texture, int u_level, int depth, int u_cell,
                           int v_cell, const int32_t *const cam[4], const int32_t screen[8], int32_t near);

#endif
