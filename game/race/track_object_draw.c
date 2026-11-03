#include "game/race/track_object_draw.h"

#include "game/gfx/x86_int.h"
#include "game/race/track_build.h"
#include "game/race/track_render.h"

#include <string.h>

enum {
    MODEL = 1,
    BILLBOARD = 4,
    TWO_SIDED = 6,
    MIN_DEPTH = 0x86,
    ANIMATED = 4,
    FRONT_FROM_1 = 1,
    SIDE_FROM_4 = 2
};

/* 004c2da4: the texture level per band for billboards with record +8 set */
static const uint8_t LEVEL_MARKED[TRACK_OBJECT_BANDS] = {
    3, 3, 3, 3, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0
};

/* 004c2e1c: the same for the others */
static const uint8_t LEVEL[TRACK_OBJECT_BANDS] = {
    2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0
};

int track_port_wide_board;
int track_port_close_boxes;

static int32_t rd32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

static int level_of(const uint8_t *table, int32_t band)
{
    return band >= 0 && band < TRACK_OBJECT_BANDS ? table[band] : 0;  /* port guard */
}

static int deep(const TrackObjectDraw *d, int32_t from, int32_t count)
{
    for (int32_t i = from; i < from + count; i++) {
        if (i < 0 || i >= d->object_capacity) return 0;  /* port guard */
        if (d->object_camera[i][2] < MIN_DEPTH) return 0;
    }
    return 1;
}

static void corners(const TrackObjectDraw *d, int32_t first, const int order[4], int32_t out[8])
{
    for (int i = 0; i < 4; i++) {
        out[2 * i] = d->object_screen[first + order[i]][0];
        out[2 * i + 1] = d->object_screen[first + order[i]][1];
    }
}

/* port: the same four points' camera-space rows, for ops.full_camera */
static void camera_corners(const TrackObjectDraw *d, int32_t first, const int order[4], const int32_t *out[4])
{
    for (int i = 0; i < 4; i++) out[i] = d->object_camera[first + order[i]];
}

/* one board: the exe's mapper through its screen corners, or Modern 3D's through their camera-space ones; a `near`
 * one (port) through the near call */
static void board(TrackObjectDraw *d, int alternate, int near, int32_t texture, int32_t first, const int order[4])
{
    if (near) {
        const int32_t *cam[4];
        camera_corners(d, first, order, cam);
        d->ops.port_near_camera(d->ops.user, texture, cam);
        return;
    }
    int32_t screen[8];
    corners(d, first, order, screen);
    if (!track_quad_on_screen(screen)) {
        /* port: a big board (a tree) has a corner far off to the side while its middle shows: the exe drops it whole */
        if (!track_port_wide_board || d->ops.port_near_camera == NULL) return;
        const int32_t *cam[4];
        camera_corners(d, first, order, cam);
        d->ops.port_near_camera(d->ops.user, texture, cam);
        return;
    }
    if (d->ops.full_camera != NULL) {
        const int32_t *cam[4];
        camera_corners(d, first, order, cam);
        d->ops.full_camera(d->ops.user, texture, cam);
    } else if (alternate) {
        d->ops.full_alt(d->ops.user, texture, screen);
    } else {
        d->ops.full(d->ops.user, texture, screen);
    }
}

static void draw_billboard(TrackObjectDraw *d, const uint8_t *record, const uint8_t *desc, int32_t band, int32_t at,
                           int near)
{
    int32_t texture = desc[2] + level_of(record[8] != 0 ? LEVEL_MARKED : LEVEL, band);
    if ((desc[0] & ANIMATED) != 0) {
        if (desc[8] == 0 || desc[9] == 0) return;  /* port guard: the original divides by 0 */
        texture += d->tick / desc[9] % desc[8] * 4;
    }
    static const int ORDER[4] = { 0, 1, 2, 3 };
    board(d, 0, near, texture, at, ORDER);
}

/* port: a face of the box is drawn only when the eye (the camera-space origin) is on its outer side: its outward normal `n` against a point `p` on it */
static int faces_eye(const int32_t n[3], const int32_t p[3])
{
    return (int64_t)n[0] * p[0] + (int64_t)n[1] * p[1] + (int64_t)n[2] * p[2] < 0;
}

/* port: how many of a face's four camera-space corners are in front of the near limit; a generated wall or roof with a corner behind the eye (the car beside the house:
 * the far corners are the front's carried back) goes through the near plane, and drawn whole it sweeps a wedge across the screen */
static int corners_ahead(const int32_t *const corner[4])
{
    int n = 0;
    for (int i = 0; i < 4; i++) n += corner[i][2] >= MIN_DEPTH;
    return n;
}

/* port: a corner of a clipped face: camera-space point and the texture fraction (16.16) there */
typedef struct {
    int32_t p[3], uv[2];
} FaceVertex;

enum { FACE_CLIP_MAX = 8, FACE_UV_ONE = 65536 };

/* port: the face clipped to the near limit (Sutherland-Hodgman on z >= MIN_DEPTH), its texture fractions carried along; the polygon's count (3..5) */
static int clip_face(const int32_t *const corner[4], FaceVertex out[FACE_CLIP_MAX])
{
    static const int32_t U[4] = { 0, FACE_UV_ONE, FACE_UV_ONE, 0 }, V[4] = { 0, 0, FACE_UV_ONE, FACE_UV_ONE };   /* the corners' fractions, as whole_uv's order */
    int n = 0;
    for (int i = 0; i < 4; i++) {
        const int j = (i + 1) & 3;
        const int32_t za = corner[i][2], zb = corner[j][2];
        const int a_in = za >= MIN_DEPTH, b_in = zb >= MIN_DEPTH;
        if (a_in) {
            memcpy(out[n].p, corner[i], sizeof out[n].p);
            out[n].uv[0] = U[i], out[n].uv[1] = V[i];
            n++;
        }
        if (a_in != b_in) {
            const double t = (double)(MIN_DEPTH - za) / (double)(zb - za);
            for (int k = 0; k < 3; k++) out[n].p[k] = (int32_t)((double)corner[i][k] + ((double)corner[j][k] - (double)corner[i][k]) * t);
            out[n].p[2] = MIN_DEPTH;
            out[n].uv[0] = (int32_t)((double)U[i] + ((double)U[j] - (double)U[i]) * t);
            out[n].uv[1] = (int32_t)((double)V[i] + ((double)V[j] - (double)V[i]) * t);
            n++;
        }
    }
    return n;
}

/* port: one generated face of a closed box: whole when all its corners are ahead of the near limit, clipped to it (box_clipped_camera, a fan of quads) when some are,
 * nothing when none are (it is behind the eye) */
static void box_face(TrackObjectDraw *d, int32_t texture, int roof, const int32_t *const corner[4])
{
    const int ahead = corners_ahead(corner);
    if (ahead == 4) {
        if (roof) d->ops.roof_camera(d->ops.user, texture, corner);
        else d->ops.full_camera(d->ops.user, texture, corner);
        return;
    }
    if (ahead == 0 || d->ops.box_clipped_camera == NULL) return;
    FaceVertex poly[FACE_CLIP_MAX];
    const int n = clip_face(corner, poly);
    for (int i = 1; i + 1 < n; i++) {   /* the fan from the first corner: a quad per triangle, its last corner repeated */
        const FaceVertex *const v[4] = { &poly[0], &poly[i], &poly[i + 1], &poly[i + 1] };
        const int32_t *cam[4];
        int32_t uv[8];
        for (int k = 0; k < 4; k++) cam[k] = v[k]->p, uv[2 * k] = v[k]->uv[0], uv[2 * k + 1] = v[k]->uv[1];
        d->ops.box_clipped_camera(d->ops.user, texture, roof, cam, uv);
    }
}

static double dot3(const int32_t a[3], const int32_t b[3])
{
    return (double)a[0] * b[0] + (double)a[1] * b[1] + (double)a[2] * b[2];
}

/* port: a rectangular box, not a gabled house or a tower on a base: the side's far edge is the near edge carried along one depth at the top and the bottom,
 * and the depth stands square to the front's width and height (to within an eighth of the lengths). Anything else (a roof's apex among the points) is left open:
 * the closing walls and roof of a box that is not one are wrong, a huge triangle in the sky. */
static int is_rectangular_box(const int32_t *const c[6])
{
    int32_t top[3], bottom[3], width[3], up[3], gap[3];
    for (int k = 0; k < 3; k++) {
        top[k] = c[4][k] - c[1][k], bottom[k] = c[5][k] - c[2][k];
        width[k] = c[0][k] - c[1][k], up[k] = c[0][k] - c[3][k], gap[k] = top[k] - bottom[k];
    }
    const double depth2 = dot3(top, top), width2 = dot3(width, width), up2 = dot3(up, up);
    if (depth2 <= 0.0 || width2 <= 0.0 || up2 <= 0.0) return 0;
    if (dot3(gap, gap) * 64.0 > depth2) return 0;                                  /* the top and the bottom edges carried by different depths */
    const double dw = dot3(top, width), du = dot3(top, up);
    return dw * dw * 64.0 <= depth2 * width2 && du * du * 64.0 <= depth2 * up2;
}

/* port: the two walls and the roof the L lacks, from the camera-space points of its six corners (0..3 the front's, 4 and 5 the side's far edge): the far
 * corners of the other two walls are the front's left corners carried along the side (by the side's depth). Each wall seen from outside, so its texture runs the
 * right way round (the owner: mirrored from the first try, so the windows run on from the neighbouring face at the corner): the back's left is its far
 * corner, the left wall's left is the front's edge. */
static void close_box(TrackObjectDraw *d, const uint8_t *desc, int level, int32_t at)
{
    const int32_t *c[6];
    for (int i = 0; i < 6; i++) c[i] = d->object_camera[at + i];
    if (!is_rectangular_box(c)) return;
    int32_t far_top[3], far_bottom[3];
    for (int k = 0; k < 3; k++) {
        far_top[k] = c[0][k] + (c[4][k] - c[1][k]);
        far_bottom[k] = c[3][k] + (c[5][k] - c[2][k]);
    }
    const int32_t *back[4] = { far_top, c[4], c[5], far_bottom };
    const int32_t *left[4] = { c[0], far_top, far_bottom, c[3] };
    const int32_t *roof[4] = { far_top, c[4], c[1], c[0] };
    /* a wall or a roof seen from inside, or from beneath, is the far side of a solid: left out (the exe's front and side keep the exe's own culling) */
    const int32_t depth[3] = { c[4][0] - c[1][0], c[4][1] - c[1][1], c[4][2] - c[1][2] };       /* from the front toward the back */
    const int32_t outward[3] = { c[0][0] - c[1][0], c[0][1] - c[1][1], c[0][2] - c[1][2] };     /* toward the left wall's side */
    const int32_t up[3] = { c[0][0] - c[3][0], c[0][1] - c[3][1], c[0][2] - c[3][2] };
    if (faces_eye(depth, c[4])) box_face(d, desc[2] + level, 0, back);
    if (faces_eye(outward, c[0])) box_face(d, desc[3] + level, 0, left);
    if (d->ops.roof_camera != NULL && faces_eye(up, c[0])) box_face(d, desc[2] + level, 1, roof);
}

static void draw_two_sided(TrackObjectDraw *d, const uint8_t *desc, int32_t band, int32_t at, int near)
{
    if (!deep(d, at + 4, 2)) {
        if (d->ops.port_near_camera == NULL) return;
        near = 1;
    }
    /* With flag 1 the original builds the front on its stack and reads the
     * side's points 4 and 5 past that copy; no track has it (all 66 two-sided
     * descriptors have flags 0), the port takes them from the screen. */
    static const int FRONT[2][4] = { { 0, 1, 2, 3 }, { 1, 2, 3, 0 } };
    static const int SIDE[2][4] = { { 1, 4, 5, 2 }, { 4, 5, 2, 1 } };
    const int level = level_of(LEVEL, band);
    board(d, 1, near, desc[2] + level, at, FRONT[desc[0] & FRONT_FROM_1]);
    const uint32_t before = d->side_face;
    d->side_face = 1;
    board(d, 1, near, desc[3] + level, at, SIDE[(desc[0] & SIDE_FROM_4) != 0]);
    d->side_face |= before;
    if (track_port_close_boxes && d->ops.full_camera != NULL && !near && at >= 0 && at + 6 <= d->object_capacity) close_box(d, desc, level, at);
}

/* The walker one record back; before the first, the last on a circuit
 * unless it is on `node` (the drawn record's), else none. */
static void previous_object(TrackObjectWalk *w, int32_t node)
{
    if (--w->cursor >= 0) {
        w->node = rd32(w->records + (uint32_t)w->cursor * TRACK_WALK_RECORD_SIZE);
        return;
    }
    w->cursor = w->record_count - 1;
    if (w->circuit == 0) {
        w->node = -1;
        return;
    }
    w->node = rd32(w->records + (uint32_t)w->cursor * TRACK_WALK_RECORD_SIZE);
    if (w->node == node) w->node = -1;
}

/* 0044a4b0 */
void track_object_draw(TrackObjectDraw *d, int32_t band, int visible)
{
    TrackObjectWalk *w = d->walk;
    const uint8_t *record = w->records + (uint32_t)w->cursor * TRACK_WALK_RECORD_SIZE;
    const uint8_t *desc = w->descriptors + (uint32_t)record[4] * TRACK_WALK_RECORD_SIZE;
    const int32_t node = rd32(record);
    if (desc[1] == MODEL) {
        const int32_t at = --w->vertex_cursor;
        const uint8_t *n = d->nodes + ((uint32_t)w->node & w->node_mask) * TRACK_BUILD_NODE_SIZE;
        const int32_t heading = (int16_t)(n[0x18] | n[0x19] << 8);
        if (at >= 0 && at < d->object_capacity) {  /* port guard */
            const int32_t position[3] = { d->object_world[at][0], d->object_world[at][1], d->object_world[at][2] };
            d->ops.model(d->ops.user, desc[2], position, x_add((int32_t)record[5] << 16, x_shl(heading, 10)));
        }
    } else {
        const int32_t at = w->vertex_cursor = x_sub(w->vertex_cursor, desc[1]);
        const int near = !deep(d, at, 4);
        if (visible && (!near || (d->ops.port_near_camera != NULL && at >= 0 && at + desc[1] <= d->object_capacity))) {
            if (desc[1] == BILLBOARD) draw_billboard(d, record, desc, band, at, near);
            else if (desc[1] == TWO_SIDED) draw_two_sided(d, desc, band, at, near);
        }
    }
    previous_object(w, node);
}
