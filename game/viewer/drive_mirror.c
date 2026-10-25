/* `--drive`'s rear-view mirror (drive_mirror.h). */
#include "game/viewer/drive_mirror.h"

#include "game/gfx/blit.h"
#include "game/gfx/flat.h"
#include "game/gfx/raster_record.h"
#include "game/gfx/shape_flip.h"
#include "game/race/car_shadow.h"
#include "game/race/mirror_sky.h"
#include "game/race/race_view.h"
#include "game/race/track_geometry.h"
#include "game/race/track_render.h"
#include "game/race/track_step.h"
#include "game/sys/bands.h"
#include "game/viewer/follow_arm.h"
#include "game/viewer/follow_behind.h"
#include "game/viewer/modern_road.h"
#include "game/viewer/screen_layout.h"

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
    BANK_HEADER = 0x10, BANK_ENTRY = 8,
    FOLLOW_MIRROR_SEGMENTS = 6, FOLLOW_MIRROR_AHEAD_ABOVE = 0x360000, FOLLOW_MIRROR_BEHIND_BELOW = 0x4a0000   /* 76 and 104 degrees */
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

enum { ON_DASH_Y = 240 };  /* port: a mirror box from here down is on the dash, not at the windshield's top */
static int widened;  /* port: drive_mirror_widen's, kept for the pictures made after it (a cockpit reloaded) */

/* port: the picture remade `extra` columns wider than the cockpit's box */
static void make_wide(DriveMirror *m, int32_t extra)
{
    const int32_t w = m->width - m->extra + extra;
    uint8_t *p = make_picture(NULL, w, m->height, 0);
    if (p == NULL) return;  /* the picture as it was */
    mirror_free(free_picture, NULL, (void **)&m->picture);
    m->picture = p, m->width = w, m->extra = extra;
}

int drive_mirror_start(DriveMirror *m, const Cockpit *c)
{
    memset(m, 0, sizeof *m);
    m->first_segment = -1;
    const int32_t box[4] = { c->dash_points[0].x, c->dash_points[0].y, c->dash_points[1].x, c->dash_points[1].y };
    m->picture = mirror_create(make_picture, NULL, box, &m->width, &m->height);
    m->on_dash = box[1] >= ON_DASH_Y;
    if (m->picture != NULL && widened && !m->on_dash) make_wide(m, m->width / 2);
    return m->picture != NULL;
}

int drive_mirror_start_own(DriveMirror *m, int32_t percent)
{
    memset(m, 0, sizeof *m);
    m->first_segment = -1;
    ViewLayout l = race_view_layouts[HIGH_MODE][race_view_rows[HIGH_MODE][MIRROR_VIEW].layouts[0]];
    l.y = l.y * percent / 100, l.width = l.width * percent / 100, l.height = l.height * percent / 100;
    l.unknown_20 = l.unknown_20 * percent / 100, l.unknown_24 = l.unknown_24 * percent / 100;
    const int32_t box[4] = { 0, 0, l.width - 1, l.height - 1 };
    m->picture = mirror_create(make_picture, NULL, box, &m->width, &m->height);
    m->wide_layout = l, m->own = 1;
    return m->picture != NULL;
}

void drive_mirror_widen(DriveMirror *m, int on)
{
    widened = on;
    if (m->picture != NULL) make_wide(m, on && !m->on_dash ? (m->width - m->extra) / 2 : 0);
}

void drive_mirror_zoom(DriveMirror *m, int32_t zoom)
{
    m->zoom = zoom > 0 ? zoom : 0;
}

/* port (render scale): `big` made (or remade) for the zoom; 0 when it cannot be, the picture then drawn as ever */
static int make_big(DriveMirror *m)
{
    const int32_t w = (int32_t)(((int64_t)m->width * m->zoom + 0x8000) >> 16), h = (int32_t)(((int64_t)m->height * m->zoom + 0x8000) >> 16);
    if (m->big != NULL && m->big_w == w && m->big_h == h) return 1;
    free(m->big);
    m->big = make_picture(NULL, w, h, 0);
    m->big_w = m->big != NULL ? w : 0, m->big_h = m->big != NULL ? h : 0;
    return m->big != NULL;
}

/* the picture's rows and columns taken from `big` (nearest): what the 2D layer's copy of the mirror holds */
static void sample_down(const DriveMirror *m)
{
    for (int32_t y = 0; y < m->height; y++) {
        const uint8_t *from = m->big + SHAPE_HEADER + (size_t)((int64_t)y * m->big_h / m->height) * m->big_w;
        uint8_t *to = m->picture + SHAPE_HEADER + (size_t)y * m->width;
        for (int32_t x = 0; x < m->width; x++) to[x] = from[(int64_t)x * m->big_w / m->width];
    }
}

void drive_mirror_free(DriveMirror *m)
{
    free(m->big);
    m->big = NULL, m->big_w = m->big_h = 0;
    mirror_free(free_picture, NULL, (void **)&m->picture);
    free(m->horz);
    m->horz = NULL;
}

void drive_mirror_place(const DriveMirror *m, const QuadTarget *target, int32_t x, int32_t y)
{
    if (m->picture == NULL || m->state == 0) return;
    if (m->extra == 0) {
        blit_shape_at(target, m->picture, x, y);  /* 0044c060: 004737d0 */
        return;
    }
    const int32_t w = m->width - m->extra, from = m->extra / 2;  /* port: the widened picture's middle */
    for (int32_t r = 0; r < m->height; r++) {
        const int32_t dy = y + r;
        if (dy < target->clip_top || dy >= target->clip_bottom) continue;
        for (int32_t c = 0; c < w; c++) {
            const int32_t dx = x + c;
            if (dx >= target->clip_left && dx < target->clip_right)
                target->pixels[dy * target->pitch + dx] = m->picture[SHAPE_HEADER + r * m->width + from + c];
        }
    }
}

typedef DriveMirrorFrame Frame;

static uint8_t node_property(void *ctx, int32_t node)
{
    const Tri *tri = ((const Frame *)ctx)->w->tri;
    TriNode n;
    return tri_node(tri, (uint32_t)node & (tri->ring_nodes - 1u), &n) == TRI_OK ? n.property : 0;
}

static void step_none(void *ctx, int32_t segment) { (void)ctx, (void)segment; }
static void build_none(void *ctx, int32_t node, int32_t segment, int32_t count) { (void)ctx, (void)node, (void)segment, (void)count; }

static void mirror_row_op(void *ctx, int32_t segment)
{
    Frame *f = ctx;
    const MirrorRowOps ops = { f, node_property };
    mirror_row(&ops, f->cache, f->segment_mask, segment, f->m->world, &f->m->count, segment_bytes);
}

/* 0043f540: the cache and list part ran before (w->track; 0043e300 and
 * 0043f48c empty here); the mirror's rows from the same cache while chase
 * +0x3c is set (004046b0 as the camera's node changes, 0043f670, 0043f51c). */
static void mirror_rows(Frame *f)
{
    const RaceState *race = f->w->race;
    int32_t w4c53a4 = 0;
    /* port: the rows checked every frame (they are made again only when their first segment moves), as the bumper
     * mirror's: a frame between two steps (drive_interp) shows the older node and took the flag the newer one set,
     * which left the rows a segment behind the scene until the next node and the nearest road out of the mirror */
    int32_t again = 1;
    const TrackStepInputs in = { race->chase.node, &again, race->chase.node, &f->m->first_segment, &w4c53a4,
                                 &f->m->count, race->circuit, f->segment_mask, 0, 1 };
    const TrackStepOps ops = { f, step_none, build_none, mirror_row_op };
    track_stream_step(&ops, &in);
    *f->w->rows_again = 0;  /* the chase's flag cleared as 0043f540 does */
}

static void view_op(void *ctx, int rear)
{
    Frame *f = ctx;
    if (!rear) return;  /* the front view is the port's own (ported_view.c) */
    static RaceCamera camera;
    camera.fov = f->m->extra != 0 ? f->m->fov : MIRROR_FOV, camera.layout_row = f->layout;
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

/* port (render bands P4): from here to the flip the mirror's draws are recorded and replayed by bands, once; the
 * picture is its own buffer, so the main view's recorder is not active (stage 3 runs before stage 6) */
static struct RasterRecorder mirror_recorder;

static void select_op(void *ctx)  /* the picture is Frame.target throughout */
{
    if (bands_threads() == 0 || raster_active != NULL) return;
    raster_record_set_runner(bands_rows);
    raster_record_begin(&mirror_recorder, ((Frame *)ctx)->target.pixels);
}

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
                                 (int16_t)race->frontend_track, 0, f->target.clip_right, f->target.clip_bottom, f->m->horz };
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

/* port: Modern 3D's cell (ported_view.c's draw_camera) */
static void draw_camera(void *user, const View *view, const FshEntry *texture, const int32_t uv[8],
                        const int32_t *const cam[4])
{
    modern_road_cell(user, view, texture, uv, cam, texture->type == TRACK_TEXTURE_TRANSPARENT_TYPE);
}

static void scene_subdivide(void *ctx, const FshEntry *t, int32_t u_level, int32_t u_cell, const int32_t *const cam[4],
                            const int32_t screen[8], int32_t near)
{
    Frame *f = ctx;
    /* The last band (the 5th segment's row to a 6th that 0043f3a8 never made) has corners past the count's points:
     * the original reads on into other globals there (00509780 the front view's vertex count, 00509784, the
     * scenery pointer 00509788), the port into zeros, which put corners at (0, 0) inside the mirror; whenever the
     * camera has just entered a segment the band filled the mirror. The port draws none of them. */
    for (int i = 0; i < 4; i++)
        if (cam[i] >= f->m->vertices + 3 * f->m->count) return;
    const DrawContext dc = { &f->m->view, { f->target.clip_left, f->target.clip_top, f->target.clip_right, f->target.clip_bottom },
                             WORLD_SHIFT, { f->w->near_dir[0], f->w->near_dir[1], f->w->near_dir[2] },
                             { &f->target, draw_uv, NULL, NULL, NULL, NULL, NULL,
                               f->w->port_modern ? draw_camera : NULL, NULL }, 0, 0 };
    mirror_draw_subdivided(&dc, t, u_level, 0, u_cell, 0, cam, screen, near);
}

static void no_contact(void *ctx, Car *car) { (void)ctx, (void)car; }

static void no_skids(void *ctx, Car *car, const int32_t f[3], const int32_t s[3], const int32_t p[3])
{
    (void)ctx, (void)car, (void)f, (void)s, (void)p;
}

/* 0045a724 pass 1: the heading from the body matrix for cars with one, then 0045a3e8 stores the camera-space place
 * and gives 0 (neither op runs): no shadow in the mirror. */
static void mirror_prepare(void *ctx, Car *car, int mirror)
{
    Frame *f = ctx;
    const ModelView mv = { &f->m->view, f->w->race->chase.position, f->m->matrix,
                           model_draw_half_side(f->w->race->track, f->w->race->track_variant), 0 };
    const CarShadowOps ops = { NULL, no_contact, no_skids };
    int32_t screen[8];
    car_prepare(&ops, car, &mv, mirror, 0, &f->target, NULL, screen);
}

static void car_draw_op(void *ctx, Car *car, int mirror)
{
    Frame *f = ctx;
    (void)mirror;
    if (f->w->draw_car == NULL) return;
    const ModelView mv = { &f->m->view, f->w->race->chase.position, f->m->matrix,
                           model_draw_half_side(f->w->race->track, f->w->race->track_variant), f->w->port_modern };
    f->w->draw_car(f->w->user, car, &mv, &f->target);
}

/* 0045a8fc */
static void scene_cars(void *ctx, int32_t node)
{
    Frame *f = ctx;
    const RaceState *race = f->w->race;
    const MirrorCarsOps ops = { f, mirror_prepare, car_draw_op };
    const MirrorCarsInputs in = { race->chase.node, race->segments, f->m->car_rows, race->drawn, race->drawn_count,
                                  f->w->followed, race->chase.kind };
    mirror_cars(&ops, &in, node);
}

/* port (port_follow_camera): the mirror's full-quad draws (the rails' and the scenery boards') and its cars on a row, for
 * the pass behind the camera (follow_behind.h) */
static Frame *follow_frame;

static void follow_full(void *user, const FshEntry *texture, const int32_t screen[8])
{
    quad_draw_full_tri(user, texture, screen, texture->type == TRACK_TEXTURE_TRANSPARENT_TYPE);
}

static void follow_full_alt(void *user, const FshEntry *texture, const int32_t screen[8])
{
    quad_draw_full(user, texture, screen, texture->type == TRACK_TEXTURE_TRANSPARENT_TYPE);
}

static void follow_cars(void *user, int32_t node)
{
    (void)user;
    scene_cars(follow_frame, node);
}

/* port (port_follow_camera): the mirror looks back from the car's heading, whichever way the car points along the track: the
 * segments behind the camera's node when the car faces down the track, those ahead of it when it faces back along it (both
 * across the track), by the pass behind the camera at the far level of detail, with the rails, scenery and cars on its rows */
static void follow_scene(Frame *f)
{
    const RaceState *race = f->w->race;
    const DrawContext dc = { &f->m->view, { f->target.clip_left, f->target.clip_top, f->target.clip_right, f->target.clip_bottom },
                             WORLD_SHIFT, { f->w->near_dir[0], f->w->near_dir[1], f->w->near_dir[2] },
                             { &f->target, draw_uv, follow_full, follow_full_alt, NULL, follow_cars, NULL,
                               f->w->port_modern ? draw_camera : NULL, NULL },
                             f->m->near, 0 };
    TrackCamera cam = { { race->chase.position[0], race->chase.position[1], race->chase.position[2] }, { 0 }, race->chase.node };
    memcpy(cam.matrix, f->m->matrix, sizeof cam.matrix);
    int32_t off = (int32_t)(((uint32_t)f->yaw - (uint32_t)race_node(race, race->chase.node).orientation * 0x400u) & 0xffffff);
    if (off > 0x800000) off = 0x1000000 - off;
    const FollowArm arm = { f->w->track, &cam, &dc, tri_is_circuit(f->w->tri), 0, 0, 1 };
    follow_frame = f;
    follow_behind_draw(&arm, off < FOLLOW_MIRROR_BEHIND_BELOW ? FOLLOW_MIRROR_SEGMENTS : 0,
                       off > FOLLOW_MIRROR_AHEAD_ABOVE ? FOLLOW_MIRROR_SEGMENTS : 0);
    follow_frame = NULL;
}

static void scene_op(void *ctx, int32_t count)
{
    Frame *f = ctx;
    if (f->w->race->port_follow_camera) {
        follow_scene(f);
        return;
    }
    const Tri *tri = f->w->tri;
    const MirrorSceneInputs in = { 0, f->layout, f->m->near, f->w->race->chase.node, f->segment_mask,
                                   f->w->race->circuit, (int32_t)(tri->ring_nodes - 1u), segment_bytes, f->cache,
                                   f->m->vertices, 1 };
    const MirrorSceneOps ops = { f, scene_texture, scene_subdivide, scene_cars };
    mirror_scene(&ops, &in, count);
}

static void flip_op(void *ctx)
{
    Frame *f = ctx;
    if (raster_active == &mirror_recorder) raster_record_end(&mirror_recorder);
    if (f->target.pixels == f->m->picture + SHAPE_HEADER) {
        shape_flip(f->m->picture);
        return;
    }
    shape_flip(f->m->big);   /* port (render scale): the larger picture, then the 2D layer's copy from it */
    sample_down(f->m);
}

int drive_mirror_begin(DriveMirror *m, const DriveMirrorWorld *w, DriveMirrorFrame *f)
{
    if (m->picture == NULL || w->tri->ring_segments > RING_SEGMENTS_MAX) return 0;
    const RaceState *race = w->race;
    const ViewRow *row = &race_view_rows[HIGH_MODE][MIRROR_VIEW];
    const int32_t size = race->window_size >= 0 && race->window_size < VIEW_SIZES ? race->window_size : 0;
    *f = (Frame){ m, w, { m->picture + SHAPE_HEADER, m->width, 0, 0, m->width, m->height },
                  &race_view_layouts[HIGH_MODE][row->layouts[size]], NULL, 0, race->chase.angles[0], race->chase.angles[1],
                  race->chase.angles[2] };
    if (race->port_follow_camera && w->followed != NULL) f->yaw = w->followed->yaw & 0xfffc00;   /* port: behind the car, not the camera */
    f->cache = w->track->cache, f->segment_mask = (int32_t)w->track->build.segment_mask;
    if (m->extra != 0) {  /* port: the layout as wide as the picture, the field widened at the same focal length */
        m->wide_layout = *f->layout;
        m->wide_layout.width += m->extra, m->wide_layout.unknown_20 += m->extra;
        m->fov = screen_fov_y(MIRROR_FOV, f->layout->width, m->wide_layout.width, 0);  /* any axis: same sum */
        f->layout = &m->wide_layout;
    }
    if (m->own) f->layout = &m->wide_layout;  /* port: the own mirror's, scaled */
    if (m->zoom > 0 && make_big(m)) {  /* port (render scale): the same camera over more pixels */
        ViewLayout *l = &m->big_layout;
        *l = *f->layout;
        const int64_t gx = m->big_w, gy = m->big_h, dx = m->width, dy = m->height;
        l->left = (int32_t)(l->left * gx / dx), l->x = (int32_t)(l->x * gx / dx), l->width = (int32_t)(l->width * gx / dx);
        l->unknown_20 = (int32_t)(l->unknown_20 * gx / dx);
        l->top = (int32_t)(l->top * gy / dy), l->y = (int32_t)(l->y * gy / dy), l->height = (int32_t)(l->height * gy / dy);
        l->unknown_24 = (int32_t)(l->unknown_24 * gy / dy);
        f->layout = l;
        f->target = (QuadTarget){ m->big + SHAPE_HEADER, m->big_w, 0, 0, m->big_w, m->big_h };
    }
    mirror_rows(f);
    /* m->car_rows: 00459d70's mirror part (00459cc4), at 0044ce3f (ported_view_frame_rows) */
    return 1;
}

MirrorFrameOps drive_mirror_ops(DriveMirrorFrame *f)
{
    return (MirrorFrameOps){ f, view_op, transform_op, select_op, sky_op, scene_op, flip_op };
}
