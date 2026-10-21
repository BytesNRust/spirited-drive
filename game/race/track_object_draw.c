#include "game/race/track_object_draw.h"

#include "game/gfx/x86_int.h"
#include "game/race/track_build.h"
#include "game/race/track_render.h"

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
    if (!track_quad_on_screen(screen)) return;
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
