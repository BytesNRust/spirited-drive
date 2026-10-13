/* `--drive`'s rear-view mirror (drive_mirror.h). */
#include "game/viewer/drive_mirror.h"

#include "game/gfx/blit.h"
#include "game/gfx/flat.h"
#include "game/gfx/shape_flip.h"
#include "game/race/mirror_sky.h"
#include "game/race/race_view.h"
#include "game/race/track_geometry.h"
#include "game/race/track_render.h"
#include "game/race/track_step.h"

#include <stdlib.h>
#include <string.h>

enum {
    SHAPE_HEADER = 0x10, SHAPE_TYPE_8BIT = 0x7b,
    WORLD_SHIFT = 11,           /* 004c52bc */
    NEAR_PLANE = 62259,         /* 004c52c0 */
    HIGH_MODE = 1,              /* 00512018 */
    MIRROR_VIEW = 4,            /* 004b78e8's view row */
    MIRROR_FOV = 70,            /* 004b78e8 +0 */
    RING_SEGMENTS_MAX = 0x8000,
    BANK_HEADER = 0x10, BANK_ENTRY = 8
};

/* 00504228: 6 bytes a segment, the mirror's LOD at +1 (0043f3a8). */
static uint8_t segment_bytes[6 * RING_SEGMENTS_MAX];

static void put16(uint8_t *b, int32_t v)
{
    b[0] = (uint8_t)v, b[1] = (uint8_t)((uint32_t)v >> 8);
}

static void put32(uint8_t *b, int32_t v)
{
    for (int i = 0; i < 4; i++) b[i] = (uint8_t)((uint32_t)v >> (8 * i));
}

static void *make_picture(void *ctx, int32_t width, int32_t height, int32_t flags)
{
    (void)ctx, (void)flags;
    if (width <= 0 || height <= 0) return NULL;
    uint8_t *p = calloc(1, SHAPE_HEADER + (size_t)width * (size_t)height);
    if (p == NULL) return NULL;
    p[0] = SHAPE_TYPE_8BIT;
    put16(p + 4, width), put16(p + 6, height);
    return p;
}

static void free_picture(void *ctx, void *picture)
{
    (void)ctx;
    free(picture);
}

/* The track's 'horz' as a bank of one shape, for 0044bddc's 004732dd. */
static uint8_t *horz_bank(const TrackArt *art)
{
    if (!art->has_horizon) return NULL;
    const FshEntry *h = &art->horizon;
    const size_t pixels = (size_t)h->width * h->height, at = BANK_HEADER + BANK_ENTRY;
    uint8_t *bank = calloc(1, at + SHAPE_HEADER + pixels);
    if (bank == NULL) return NULL;
    memcpy(bank, "SHPI", 4);
    put32(bank + 4, (int32_t)(at + SHAPE_HEADER + pixels)), put32(bank + 8, 1);
    memcpy(bank + BANK_HEADER, "horz", 4), put32(bank + BANK_HEADER + 4, (int32_t)at);
    uint8_t *shape = bank + at;
    shape[0] = h->type;
    put16(shape + 4, h->width), put16(shape + 6, h->height);
    for (int i = 0; i < 4; i++) put16(shape + 8 + 2 * i, h->misc[i]);
    memcpy(shape + SHAPE_HEADER, h->payload, pixels <= h->payload_size ? pixels : h->payload_size);
    return bank;
}

int drive_mirror_start(DriveMirror *m, const Cockpit *c)
{
    memset(m, 0, sizeof *m);
    m->first_segment = -1;
    const int32_t box[4] = { c->dash_points[0].x, c->dash_points[0].y, c->dash_points[1].x, c->dash_points[1].y };
    m->picture = mirror_create(make_picture, NULL, box, &m->width, &m->height);
    return m->picture != NULL;
}

void drive_mirror_free(DriveMirror *m)
{
    mirror_free(free_picture, NULL, (void **)&m->picture);
    free(m->horz);
    m->horz = NULL;
}

void drive_mirror_place(const DriveMirror *m, const QuadTarget *target, int32_t x, int32_t y)
{
    if (m->picture != NULL && m->state != 0) blit_shape_at(target, m->picture, x, y);  /* 0044c060: 004737d0 */
}

typedef struct {
    DriveMirror *m;
    const DriveMirrorWorld *w;
    QuadTarget target;          /* the picture */
    const ViewLayout *layout;   /* the mirror camera's */
    int32_t pitch, yaw, roll;
} Frame;

static uint8_t node_property(void *ctx, int32_t node)
{
    const Tri *tri = ((const Frame *)ctx)->w->tri;
    TriNode n;
    return tri_node(tri, (uint32_t)node & (tri->ring_nodes - 1u), &n) == TRI_OK ? n.property : 0;
}

/* A segment's record as the cache holds it: its 12 header bytes, then its
 * rows of 11 points (track_geometry.h). */
static void encode_record(const Frame *f, int32_t segment)
{
    TrackSegment s;
    const uint32_t mask = f->w->tri->ring_segments - 1u;
    if (track_decode_segment(f->w->tri, (uint32_t)segment & mask, &s) != TRI_OK) memset(&s, 0, sizeof s);
    uint8_t *r = f->m->cache + mirror_segment_offset(segment, (int32_t)mask);
    r[0] = s.unknown_0c, r[1] = s.fence;
    memcpy(r + 2, s.texture, sizeof s.texture);
    for (int row = 0; row < TRACK_SEGMENT_ROWS; row++)
        for (int p = 0; p < TRACK_ROW_POINTS; p++) {
            uint8_t *at = r + 12 + 12 * (row * TRACK_ROW_POINTS + p);
            put32(at, s.rows[row][p].x), put32(at + 4, s.rows[row][p].y), put32(at + 8, s.rows[row][p].z);
        }
}

static void step_none(void *ctx, int32_t segment) { (void)ctx, (void)segment; }
static void build_none(void *ctx, int32_t node, int32_t segment, int32_t count) { (void)ctx, (void)node, (void)segment, (void)count; }

static void mirror_row_op(void *ctx, int32_t segment)
{
    Frame *f = ctx;
    encode_record(f, segment);
    const MirrorRowOps ops = { f, node_property };
    mirror_row(&ops, f->m->cache, (int32_t)(f->w->tri->ring_segments - 1u), segment, f->m->world, &f->m->count,
               segment_bytes);
}

/* 0043f540's mirror part. The port's renderer builds its own list each frame
 * (0043e300 and 0043f48c empty here); chase +0x3c is taken as set each frame,
 * so the rows follow the camera's segment. TODO(0043f540): who sets +0x3c. */
static void mirror_rows(Frame *f)
{
    const RaceState *race = f->w->race;
    int32_t again = 1, w4c53a4 = 0;
    const TrackStepInputs in = { race->chase.node, &again, race->chase.node, &f->m->first_segment, &w4c53a4,
                                 &f->m->count, race->circuit, (int32_t)(f->w->tri->ring_segments - 1u), 0 };
    const TrackStepOps ops = { f, step_none, build_none, mirror_row_op };
    track_stream_step(&ops, &in);
}

static void view_op(void *ctx, int rear)
{
    Frame *f = ctx;
    if (!rear) return;  /* the front view is the port's own (ported_view.c) */
    static RaceCamera camera;
    camera.fov = MIRROR_FOV, camera.layout_row = f->layout;
    track_camera_rear_matrix(f->pitch, f->yaw, f->roll, f->m->matrix);
    const RaceCameraPair pair = { &camera, &camera, f->m->matrix, f->m->matrix };
    static const int32_t still[2] = { 0, 0 };  /* 004f8010 / 004f8014 apply ahead only */
    const RaceCamera *picked;
    const int32_t *m;
    race_camera_view(&f->m->view, &pair, 1, still, 0, &picked, &m);
}

static void transform_op(void *ctx)
{
    Frame *f = ctx;
    const MirrorTransformInputs in = { NEAR_PLANE, WORLD_SHIFT, f->w->race->chase.position, f->m->matrix, &f->m->view,
                                       f->m->count, (const int32_t (*)[3])f->m->world };
    mirror_transform(&in, &f->m->near, f->m->vertices);
}

static void select_op(void *ctx) { (void)ctx; }  /* the picture is Frame.target throughout */

static void sky_fill(void *ctx, uint32_t colour)
{
    const QuadTarget *t = &((Frame *)ctx)->target;
    flat_fill_rect(t, t->clip_left, t->clip_top, t->clip_right - t->clip_left, t->clip_bottom - t->clip_top, colour);
}

static void sky_rect(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t colour)
{
    flat_fill_rect(&((Frame *)ctx)->target, x, y, w, h, colour);
}

static void sky_quad(void *ctx, const uint8_t *shape, const int32_t points[8])
{
    (void)shape;  /* the bank's only shape: the track's 'horz' */
    Frame *f = ctx;
    quad_draw_full_tri(&f->target, &f->w->art->horizon, points, 0);  /* 00486720 */
}

static void sky_op(void *ctx)
{
    Frame *f = ctx;
    if (f->m->horz == NULL) f->m->horz = horz_bank(f->w->art);
    if (f->m->horz == NULL) return;
    const RaceState *race = f->w->race;
    const ViewRow *row = &race_view_rows[HIGH_MODE][MIRROR_VIEW];
    const MirrorSkyInputs in = { race->chase.node, f->pitch, f->yaw, HIGH_MODE, row->unknown_14[3], f->layout->y,
                                 f->m->enclosed_colour, f->m->property8_colour,
                                 (int16_t)race->frontend_track, 0, f->m->width, f->m->height, f->m->horz };
    const MirrorSkyOps ops = { f, node_property, sky_fill, sky_rect, sky_quad };
    mirror_sky(&ops, &in);
}

static const FshEntry *scene_texture(void *ctx, int32_t flat)
{
    const TrackArt *art = ((Frame *)ctx)->w->art;
    if (flat < 0 || flat >= TRACK_TEXTURE_SLOTS * TRACK_TEXTURE_VARIANTS) return NULL;
    const TrackTexture *t = &art->slots[flat / TRACK_TEXTURE_VARIANTS][flat % TRACK_TEXTURE_VARIANTS];
    return t->present ? &t->bitmap : NULL;
}

static void draw_uv(void *user, const FshEntry *texture, const int32_t uv[8], const int32_t screen[8])
{
    quad_draw(user, texture, uv, screen, texture->type == TRACK_TEXTURE_TRANSPARENT_TYPE);  /* 004f9e94 */
}

static void scene_subdivide(void *ctx, const FshEntry *t, int32_t u_level, int32_t u_cell, const int32_t *const cam[4],
                            const int32_t screen[8], int32_t near)
{
    Frame *f = ctx;
    const DrawContext dc = { &f->m->view, { f->target.clip_left, f->target.clip_top, f->target.clip_right, f->target.clip_bottom },
                             WORLD_SHIFT, { f->w->near_dir[0], f->w->near_dir[1], f->w->near_dir[2] },
                             { &f->target, draw_uv, NULL, NULL, NULL, NULL } };
    mirror_draw_subdivided(&dc, t, u_level, 0, u_cell, 0, cam, screen, near);
}

/* 0045a724: the heading from the body matrix for cars with one (0042ebbc; car_draw draws those by the matrix), then
 * 0045a3e8 with pass 1, which only stores the camera-space place (car_draw's own) and returns 0: no shadow
 * (00482794) in the mirror. */
static void car_prepare(void *ctx, Car *car, int mirror) { (void)ctx, (void)car, (void)mirror; }

static void car_draw_op(void *ctx, Car *car, int mirror)
{
    Frame *f = ctx;
    (void)mirror;
    if (f->w->draw_car == NULL) return;
    const ModelView mv = { &f->m->view, f->w->race->chase.position, f->m->matrix, 0 };
    f->w->draw_car(f->w->user, car, &mv, &f->target);
}

/* 0045a8fc. TODO(004414ea): 00509e8c / 00509e14 (the cars drawn) taken as the race's cars. */
static void scene_cars(void *ctx, int32_t node)
{
    Frame *f = ctx;
    const RaceState *race = f->w->race;
    const MirrorCarsOps ops = { f, car_prepare, car_draw_op };
    const MirrorCarsInputs in = { race->chase.node, race->segments, f->m->car_rows, race->cars, race->car_count,
                                  f->w->followed, race->chase.kind };
    mirror_cars(&ops, &in, node);
}

static void scene_op(void *ctx, int32_t count)
{
    Frame *f = ctx;
    const Tri *tri = f->w->tri;
    const MirrorSceneInputs in = { 0, f->layout, f->m->near, f->w->race->chase.node, (int32_t)(tri->ring_segments - 1u),
                                   f->w->race->circuit, (int32_t)(tri->ring_nodes - 1u), segment_bytes, f->m->cache,
                                   f->m->vertices };
    const MirrorSceneOps ops = { f, scene_texture, scene_subdivide, scene_cars };
    mirror_scene(&ops, &in, count);
}

static void flip_op(void *ctx) { shape_flip(((Frame *)ctx)->m->picture); }

void drive_mirror_frame(DriveMirror *m, const DriveMirrorWorld *w)
{
    if (m->picture == NULL || w->tri->ring_segments > RING_SEGMENTS_MAX) return;
    const RaceState *race = w->race;
    const ViewRow *row = &race_view_rows[HIGH_MODE][MIRROR_VIEW];
    const int32_t size = race->window_size >= 0 && race->window_size < VIEW_SIZES ? race->window_size : 0;
    Frame f = { m, w, { m->picture + SHAPE_HEADER, m->width, 0, 0, m->width, m->height },
                &race_view_layouts[HIGH_MODE][row->layouts[size]], race->chase.angles[0], race->chase.angles[1],
                race->chase.angles[2] };
    mirror_rows(&f);
    /* 00459d70's mirror part (00459cc4), at 0044ce3f */
    mirror_car_rows(race->cars, race->car_count, race->segments, race->chase.node, m->car_rows);
    const MirrorFrameOps ops = { &f, view_op, transform_op, select_op, sky_op, scene_op, flip_op };
    /* TODO(0044cc74): 004b91c8 (2 or more keeps the mirror's last picture) and 004fb787 are 0 in --drive */
    const MirrorFrameInputs in = { race->chase.unknown_68, 0, 0, MIRROR_SEGMENTS };
    mirror_frame(&ops, &in, &m->state);
}
