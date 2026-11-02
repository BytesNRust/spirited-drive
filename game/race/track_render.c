#include "game/race/track_render.h"

#include "game/race/race_detail.h"
#include "game/race/track_cache.h"
#include "game/race/track_geometry.h"
#include "game/race/track_near.h"
#include "game/race/track_tables.h"
#include "game/gfx/x86_int.h"
#include "game/sys/fixed_math.h"

#include <string.h>

enum {
    ROW_POINTS = TRACK_ROW_POINTS,
    NEAR_PLANE = 62259,      /* 004c52c0, world 16.16 */
    CLAMP_X = 5376,          /* 004c52c8 (004c52c4 holds 0xa80000, 5376 << 11, not read here) */
    CLAMP_Y = 5376,          /* 004c52cc */
    MIN_DIR_Z = 0x1999,      /* 004497d8 */
    NODES_AT = 0x98C,        /* the header's nodes (0043e4e0: 00505038) */
    WINDOW_AHEAD = 0x1E,     /* 005041e8, 0043e4e0 */
    WINDOW_BEHIND = WINDOW_AHEAD + 5,  /* 005041e4 */
    WINDOW_HEAD = -9999,     /* 005041dc, 0043dca4 */
    STAGE_RING = 0x8000      /* 0050420c on a stage */
};

/* ---- camera matrix (004496ac) ---- */

void track_camera_matrix(int32_t pitch, int32_t yaw, int32_t roll, int mirrored, int32_t vertical_scale, int32_t out[9])
{
    int32_t x[9], y[9], z[9], t[9], u[9], s[9], v[9];
    mat3_rotate_x(x, pitch);
    mat3_rotate_z(z, roll);
    mat3_rotate_y(y, yaw);
    mat3_scale_y(s, vertical_scale);
    int32_t flip[9];
    mat3_scale_x(flip, mirrored ? -0x10000 : 0x10000);
    mat3_mul(y, z, t);
    mat3_mul(t, x, u);
    mat3_mul(u, s, v);
    mat3_mul(v, flip, out);
}

void track_camera_rear_matrix(int32_t pitch, int32_t yaw, int32_t roll, int32_t out[9])
{
    int32_t x[9], y[9], z[9], t[9], u[9], half[9];
    mat3_rotate_x(x, pitch);
    mat3_rotate_z(z, roll);
    mat3_rotate_y(y, yaw);
    mat3_rotate_y(half, 0x800000);
    mat3_mul(y, z, t);
    mat3_mul(t, x, u);
    mat3_mul(u, half, out);
}


/* ---- the original's state, the vertex list (0043e4e0's, 0043f540 / 0043f48c) ---- */

/* 0043e4e0's part the build and the draws read: the header's nodes
 * (00505038, a zeroed tail for the segments past a stage's last), the
 * descriptors (00504204) and records (00509788, the -1 after the last),
 * the knocked-over bytes (004b9098: none here, ported_view binds the race's), the masks (stages: -1). */
static void bind_track(TrackRender *r, const Tri *tri, const TrackArt *art)
{
    r->tri = tri;
    r->art = art;
    memset(r->nodes, 0, sizeof r->nodes);
    const size_t nodes = (size_t)TRI_NODE_CAPACITY * TRACK_BUILD_NODE_SIZE;
    if (tri->size >= NODES_AT + nodes) memcpy(r->nodes, tri->data + NODES_AT, nodes);
    memset(r->descriptors, 0, sizeof r->descriptors);
    const size_t descs = tri->size - tri->descriptors_offset < sizeof r->descriptors ? tri->size - tri->descriptors_offset
                                                                                    : sizeof r->descriptors;
    memcpy(r->descriptors, tri->data + tri->descriptors_offset, descs);
    const uint32_t count = tri->scenery_count < TRI_SCENERY_MAX ? tri->scenery_count : TRI_SCENERY_MAX;
    memcpy(r->records, tri->data + tri->scenery_offset, (size_t)count * TRI_SCENERY_SIZE);
    memset(r->records + (size_t)count * TRI_SCENERY_SIZE, 0xFF, TRI_SCENERY_SIZE);
    memset(r->knocked, 0, sizeof r->knocked);
    memset(r->entries, 0, sizeof r->entries);
    const int circuit = tri_is_circuit(tri);
    TrackBuild *b = &r->build;
    *b = (TrackBuild){ r->cache, r->nodes, r->knocked, circuit ? tri->ring_segments - 1u : 0xFFFFFFFFu, r->world,
                       TRACK_MAX_VERTICES, 0, 0, r->entries, r->object_world, TRACK_MAX_OBJECT_VERTICES,
                       { r->descriptors, r->records, (int32_t)count, 0, 0, 0, 0, 0, -1, 0, circuit,
                         circuit ? tri->ring_nodes - 1u : 0xFFFFFFFFu, 0 },
                       0, -1, 0, r->psx != NULL ? r->light_vertex : NULL, r->psx, r->psx != NULL ? r->tunnel_vertex : NULL };
    r->draw_segments = TRACK_DRAW_SEGMENTS;
    r->window = (TrackCacheWindow){ WINDOW_HEAD, 0, WINDOW_BEHIND, WINDOW_AHEAD,
                                    circuit ? (int32_t)tri->ring_segments : STAGE_RING,
                                    circuit ? (int32_t)tri->ring_segments - 1 : -1 };
    if (r->reach != 0) track_render_set_reach(r, r->reach);
}

void track_render_set_ring(TrackRender *r, int32_t records)
{
    const int32_t before = track_cache_records();
    track_cache_set_records(records);
    if (track_cache_records() != before) r->window.head = WINDOW_HEAD, r->build.first_node = -1;   /* loaded and built again from the camera's segment */
}

void track_render_set_light(TrackRender *r, const PsxLight *light)
{
    if (light == r->psx) return;
    r->psx = light;
    r->build.light = light;
    r->build.light_out = light != NULL ? r->light_vertex : NULL;
    r->build.tunnel_out = light != NULL ? r->tunnel_vertex : NULL;
    r->build.first_node = -1;   /* the track built again with (or without) the light */
}

/* port: the nearest distance the detail asks for (race_detail.c's NEAREST) */
enum { REACH_NEAREST = 8 };

void track_render_set_reach(TrackRender *r, int32_t segments)
{
    /* 28 is the exe's own count (no reach); 8..56 otherwise, as the detail asks for them */
    const int32_t reach = segments >= REACH_NEAREST && segments <= DRAW_SEGMENTS_ULTRA && segments != TRACK_DRAW_SEGMENTS
                              ? segments : 0;
    const int draw = reach != 0 ? reach : TRACK_DRAW_SEGMENTS;
    if (reach == r->reach && draw == r->draw_segments) return;
    /* the track built for the old count is built again at the next frame */
    if (draw != r->draw_segments) r->build.first_node = -1;
    r->reach = reach;
    r->draw_segments = draw;
    r->build.reach = r->reach;
    /* port: the objects of the farther segments too (0xb4 is what 0x1c segments place at most) */
    r->build.walk.max_objects = r->reach != 0 ? TRACK_WALK_MAX_OBJECTS * r->reach / TRACK_DRAW_SEGMENTS : 0;
    r->window.ahead = r->draw_segments + (WINDOW_AHEAD - TRACK_DRAW_SEGMENTS);
    r->window.behind = r->window.ahead + (WINDOW_BEHIND - WINDOW_AHEAD);
}

/* Port guard: past a stage's last segment the header's table points past
 * the terrain block, where the original reads beyond its buffer (see STATE,
 * open hypotheses); the port repeats the record before's last row, so the
 * polygons there have no area. */
static void stage_end_record(TrackRender *r, int32_t segment)
{
    uint8_t *at = r->cache + mirror_segment_offset(segment, (int32_t)r->build.segment_mask);
    const uint8_t *before = r->cache + mirror_segment_offset(segment - 1, (int32_t)r->build.segment_mask);
    memset(at, 0, 12);
    for (int row = 0; row < TRACK_SEGMENT_ROWS; row++)
        memmove(at + 12 + 12 * row * ROW_POINTS, before + 12 + 12 * (TRACK_SEGMENT_ROWS - 1) * ROW_POINTS, 12 * ROW_POINTS);
}

/* 00477008: the whole file is read before the race (tri_open checked every
 * chunk's tag), so the wait never runs. */
static void chunk_wait(void *ctx) { (void)ctx; }

/* 0043e244 for 0043e300 */
static void load_segment(void *ctx, int32_t segment)
{
    TrackRender *r = ctx;
    const Tri *tri = r->tri;
    const int32_t s = (int32_t)((uint32_t)segment & r->build.segment_mask);
    if (s >= 0 && (uint32_t)s >= tri->segment_count) {
        stage_end_record(r, segment);  /* port guard */
        return;
    }
    const TrackSegmentSource src = { tri->data, tri->data + tri->terrain_offset, r->nodes, r->build.segment_mask,
                                     r->build.walk.node_mask };
    const TrackCacheOps ops = { NULL, chunk_wait };
    track_cache_load(&ops, &src, tri_is_circuit(tri), r->cache, segment);
}

/* 0043f540's cache and list (its mirror part is drive_mirror.c's) */
void track_render_build(TrackRender *r, const Tri *tri, const TrackArt *art, int32_t camera_node)
{
    if (r->tri != tri) bind_track(r, tri, art);
    r->art = art;
    const int32_t segment = camera_node / 4;
    if (camera_node >= 0 && segment + r->draw_segments + 1 < TRACK_SEGMENT_ENTRIES) {  /* port guard */
        const TrackWindowOps ops = { r, load_segment };
        track_cache_window(&ops, &r->window, segment);
        if (r->build.first_node < 0 || r->build.first_node / 4 != segment)
            track_build(&r->build, camera_node, segment, r->draw_segments);
    }
    /* what 0044aa94 reads: 00509780, 005041cc (0044a804 may have reset it), 005041b4 */
    r->vertex_count = r->build.vertex_count;
    r->object_count = r->build.walk.count;
    r->object_vertex_count = r->build.walk.vertex_count;
}

/* ---- camera space and projection (0044aa94) ---- */

static int32_t mul_round(int32_t value, int32_t t)
{
    const int64_t p = (int64_t)value * t;
    return (int32_t)((uint32_t)((uint64_t)p >> 16) + (uint32_t)(((uint64_t)p >> 15) & 1));
}

/* Moves a point in front of `limit` onto it along the road direction. */
static void push_to(int32_t v[3], int32_t limit, const int32_t dir[3])
{
    if (v[2] >= limit) return;
    const int32_t gap = x_sub(limit, v[2]);
    const int32_t t = x_add((int32_t)((int64_t)(gap % dir[2]) * 65536 / dir[2]), (int32_t)((uint32_t)(gap / dir[2]) << 16));
    v[0] = x_add(v[0], mul_round(t, dir[0]));
    v[1] = x_add(v[1], mul_round(t, dir[1]));
    v[2] = limit;
}

static int32_t clamp_symmetric(int32_t v, int32_t limit)
{
    if (limit < v) return limit;
    if (v < -limit) return -limit;
    return v;
}

/* 004497d8 */
void track_near_direction(const int32_t (*camera)[3], int32_t next, int32_t dir[3])
{
    for (int k = 0; k < 3; k++) dir[k] = x_sub(camera[next][k], camera[0][k]);
    const int32_t z = dir[2] > 0 ? dir[2] : x_sub(0, dir[2]);
    if (z < MIN_DIR_Z) dir[2] = MIN_DIR_Z;
}

int32_t track_render_near(const DrawContext *ctx)
{
    const int32_t plane = ctx->port_near_scale != 0 ? (int32_t)((int64_t)NEAR_PLANE * ctx->port_near_scale >> 16)
                                                    : NEAR_PLANE;
    return plane >> ctx->world_shift;
}

/* 0044aa94 */
void track_render_transform(TrackRender *r, const TrackCamera *cam, DrawContext *ctx)
{
    const int shift = ctx->world_shift;
    const int32_t offset[3] = { x_sub(0, cam->position[0]) >> shift, x_sub(0, cam->position[1]) >> shift,
                                x_sub(0, cam->position[2]) >> shift };
    for (int i = 0; i < r->vertex_count; i++) {
        const int32_t t[3] = { x_add(r->world[i][0] >> shift, offset[0]), x_add(r->world[i][1] >> shift, offset[1]),
                               x_add(r->world[i][2] >> shift, offset[2]) };
        mat3_apply(t, cam->matrix, r->camera[i]);
    }
    /* the camera's segment's count, the short 0050422c[(segment & 00504210) x 6] */
    const TrackSegmentEntry *first = &r->build.segments[(uint32_t)(cam->node / 4) & r->build.segment_mask];
    const int next = first->count >= 0 && first->count < r->vertex_count ? first->count : 0;  /* port guard */
    track_near_direction((const int32_t (*)[3])r->camera, next, ctx->near_dir);

    /* The clamps and the near plane push work on a copy (0048ec60 into
     * 00510764), which is what is projected; the draws read r->camera. */
    memcpy(r->clamped, r->camera, (size_t)r->vertex_count * sizeof r->camera[0]);
    const int32_t near = track_render_near(ctx);
    ctx->near = near;  /* the camera record's +4 */
    const int near_count = r->vertex_count - (r->draw_segments - 3) * ROW_POINTS;
    for (int i = 0; i < r->vertex_count; i++) {
        int32_t *v = r->clamped[i];
        if (i < near_count) {
            v[0] = clamp_symmetric(v[0], v[2] < near * 2 ? CLAMP_X / 4 : CLAMP_X);
            v[1] = clamp_symmetric(v[1], CLAMP_Y);
            push_to(v, near, ctx->near_dir);
        } else {
            push_to(v, near * 8, ctx->near_dir);
        }
    }
    for (int i = 0; i < r->vertex_count; i++) view_project(ctx->view, r->clamped[i], r->screen[i]);
    if (r->object_count <= 0) return;
    for (int i = 0; i < r->object_vertex_count; i++) {
        const int32_t t[3] = { x_add(r->object_world[i][0] >> shift, offset[0]),
                               x_add(r->object_world[i][1] >> shift, offset[1]),
                               x_add(r->object_world[i][2] >> shift, offset[2]) };
        mat3_apply(t, cam->matrix, r->object_camera[i]);
        view_project(ctx->view, r->object_camera[i], r->object_screen[i]);
    }
}

/* ---- visibility (00449f90, 0044a0d0) ---- */

static const int TRIANGLES[3][3] = { { 1, 2, 3 }, { 3, 0, 1 }, { 2, 3, 0 } };  /* 004c5320.. */

/* 00449f0c / 0048f750 */
int32_t track_winding(const int32_t *s, const int tri[3], int mirrored)
{
    const int a = mirrored ? tri[2] : tri[0], b = tri[1], c = mirrored ? tri[0] : tri[2];
    const uint32_t ax = (uint32_t)s[2 * a], ay = (uint32_t)s[2 * a + 1];
    const uint32_t bx = (uint32_t)s[2 * b], by = (uint32_t)s[2 * b + 1];
    const uint32_t cx = (uint32_t)s[2 * c], cy = (uint32_t)s[2 * c + 1];
    return (int32_t)((cy - by) * (ax - bx) - (cx - bx) * (ay - by));
}

/* Facing test of either side of the road: strips 0-4 face one way. */
static int faces_camera(const int32_t *s, int strip, int triangles, int mirrored)
{
    for (int t = 0; t < triangles; t++) {
        const int32_t w = track_winding(s, TRIANGLES[t], mirrored);
        if (strip < 5 ? w < 0 : w >= 1) return 1;
    }
    return 0;
}

/* 00449f90 */
int track_visible_near(const MirrorBounds *b, const int32_t s[8], int strip, int mirrored)
{
    const int right = s[0] > b->right && s[2] > b->right && s[4] > b->right && s[6] > b->right;
    const int left = s[0] < b->left && s[2] < b->left && s[4] < b->left && s[6] < b->left;
    const int below = s[1] > b->bottom && s[7] > b->bottom;
    if (right || left || below) return 0;
    return faces_camera(s, strip, 3, mirrored);
}

/* 0044a0d0 */
int track_visible_far(const ClipRect *clip, const int32_t s[8], int strip, int mirrored)
{
    if (outcode_shared(clip, s, 4) != 0) return 0;
    return faces_camera(s, strip, 2, mirrored);
}

/* The slot table is read flat: texture * 8 + variant may run into the next
 * texture's variants, as in the original. */
static const FshEntry *texture_of(const TrackArt *art, int texture, int variant)
{
    const int flat = texture * TRACK_TEXTURE_VARIANTS + variant;
    if (flat < 0 || flat >= TRACK_TEXTURE_SLOTS * TRACK_TEXTURE_VARIANTS) return NULL;
    const TrackTexture *t = &art->slots[flat / TRACK_TEXTURE_VARIANTS][flat % TRACK_TEXTURE_VARIANTS];
    return t->present ? &t->bitmap : NULL;
}

enum { SCREEN_LIMIT = 0x801 };

/* 0044a450 */
int track_quad_on_screen(const int32_t s[8])
{
    for (int i = 0; i < 4; i++)
        if (!(s[2 * i] < SCREEN_LIMIT && -SCREEN_LIMIT < s[2 * i])) return 0;
    return 1;
}

/* 0044a1c4 */
void track_point_screen(const TrackCamera *cam, const DrawContext *ctx, const int32_t world[3], int32_t camera[3],
                        int32_t screen[2])
{
    const int shift = ctx->world_shift & 31;  /* sar by cl */
    const int32_t t[3] = { x_sub(world[0], cam->position[0]) >> shift, x_sub(world[1], cam->position[1]) >> shift,
                           x_sub(world[2], cam->position[2]) >> shift };
    mat3_apply(t, cam->matrix, camera);
    int32_t p[3] = { camera[0], camera[1], camera[2] };
    push_to(p, ctx->near, ctx->near_dir);
    view_project(ctx->view, p, screen);
}

/* ---- drawing (0044a804, 0044b510 and its callees), port glue to the sink ---- */

typedef TrackGlue Glue;

static const FshEntry *road_texture(void *ctx, int32_t flat)
{
    const Glue *g = ctx;
    return texture_of(g->r->art, flat / TRACK_TEXTURE_VARIANTS, flat % TRACK_TEXTURE_VARIANTS);
}

/* 005100c4 holds 4 dwords per texture; the level within them (004c2da4 /
 * 004c2e1c) lands on copies of the same bitmap. */
static const FshEntry *scenery_texture(const TrackArt *art, int32_t resource)
{
    const int32_t id = resource / 4;
    return id >= 0 && id < TRACK_SCENERY_TEXTURES && art->scenery[id].present ? &art->scenery[id].bitmap : NULL;
}

static void point_screen(void *ctx, const int32_t world[3], int32_t camera[3], int32_t screen[2])
{
    const Glue *g = ctx;
    track_point_screen(g->cam, g->ctx, world, camera, screen);
}

/* port (PS1 light): the polygon the road's draws paint is a lit one while light_now says so; the sink hears of it for the
 * length of the draw */
static const uint8_t *light_now_of(const Glue *g) { return g->light_now != NULL ? g->light_now : g->r->light_now; }

static void light_on(const Glue *g)
{
    const uint8_t *now = light_now_of(g);
    if (track_light_hook != NULL && now[4] != 0) track_light_tunnel = now[5], track_light_hook(now);
}

static void light_off(const Glue *g)
{
    if (track_light_hook != NULL && light_now_of(g)[4] != 0) track_light_tunnel = 0, track_light_hook(NULL);
}

static void subdivide(void *ctx, const FshEntry *t, int32_t u_level, int32_t u_cell, const int32_t *const cam[4],
                      const int32_t screen[8], int32_t near)
{
    const Glue *g = ctx;
    light_on(g);
    track_draw_subdivided(g->ctx, t, u_level, 0, u_cell, 0, cam, screen, near);
    light_off(g);
}

static void full(void *ctx, const FshEntry *t, const int32_t screen[8])
{
    const Glue *g = ctx;
    light_on(g);
    g->ctx->sink.draw_full(g->ctx->sink.user, t, screen);
    light_off(g);
}

static void full_alt(void *ctx, const FshEntry *t, const int32_t screen[8])
{
    const Glue *g = ctx;
    light_on(g);
    g->ctx->sink.draw_full_alt(g->ctx->sink.user, t, screen);
    light_off(g);
}

/* the whole texture's uv as 004f9e9c maps it */
static void whole_uv(const FshEntry *t, int32_t uv[8])
{
    const int32_t right = ((int16_t)t->width << 16) - 1, bottom = ((int16_t)t->height << 16) - 1;
    const int32_t whole[8] = { 0, 0, right, 0, right, bottom, 0, bottom };
    memcpy(uv, whole, sizeof whole);
}

/* port: Modern 3D's far road, the whole texture */
static void full_camera(void *ctx, const FshEntry *t, const int32_t *const cam[4])
{
    const Glue *g = ctx;
    int32_t uv[8];
    whole_uv(t, uv);
    light_on(g);
    g->ctx->sink.draw_camera(g->ctx->sink.user, g->ctx->view, t, uv, cam);
    light_off(g);
}

static void cars(void *ctx, int32_t node)
{
    const Glue *g = ctx;
    if (g->ctx->sink.draw_cars != NULL) g->ctx->sink.draw_cars(g->ctx->sink.user, node);
}

static void skids(void *ctx, int32_t node, int32_t row, const int32_t s[2], const int32_t t[2])
{
    const Glue *g = ctx;
    (void)node;
    if (g->ctx->sink.draw_skids != NULL) g->ctx->sink.draw_skids(g->ctx->sink.user, row, s, t);
}

static void no_alternate(void *ctx, int32_t segments, int32_t node, int32_t near)
{
    (void)ctx, (void)segments, (void)node, (void)near;
}

/* port (PS1 light): the object being drawn stands at the walker's node and takes its terrain's light */
static void object_light_on(const Glue *g)
{
    if (track_light_hook == NULL || g->r->psx == NULL) return;
    const uint8_t v = psx_light_node(g->r->psx, (uint32_t)(g->walk != NULL ? g->walk : &g->r->build.walk)->node);
    const uint8_t corners[4] = { v, v, v, v };
    track_light_hook(corners);
}

static void object_light_off(const Glue *g)
{
    if (track_light_hook != NULL && g->r->psx != NULL) track_light_hook(NULL);
}

static void object_model(void *user, int32_t model, const int32_t position[3], int32_t angle)
{
    const Glue *g = user;
    object_light_on(g);
    if (g->ctx->sink.draw_model != NULL) g->ctx->sink.draw_model(g->ctx->sink.user, model, position, (uint32_t)angle);
    object_light_off(g);
}

static void object_full(void *user, int32_t texture, const int32_t screen[8])
{
    const Glue *g = user;
    const FshEntry *t = scenery_texture(g->r->art, texture);
    object_light_on(g);
    if (t != NULL) g->ctx->sink.draw_full(g->ctx->sink.user, t, screen);
    object_light_off(g);
}

static void object_full_alt(void *user, int32_t texture, const int32_t screen[8])
{
    const Glue *g = user;
    const FshEntry *t = scenery_texture(g->r->art, texture);
    object_light_on(g);
    if (t != NULL) g->ctx->sink.draw_full_alt(g->ctx->sink.user, t, screen);
    object_light_off(g);
}

/* port: Modern 3D's scenery board, the whole texture as 004f9e9c maps it, from camera-space corners */
static void object_full_camera(void *user, int32_t texture, const int32_t *const cam[4])
{
    const Glue *g = user;
    const FshEntry *t = scenery_texture(g->r->art, texture);
    object_light_on(g);
    if (t != NULL) full_camera(user, t, cam);
    object_light_off(g);
}

/* port (track_port_close_boxes): a roof's tone, the darkest colour a facade really uses (at least one texel in fifty of it, the transparent one left out).
 * The roof is a 2 x 2 texture of that tone, kept per facade for good: the render bands replay a recorded draw later, by the texture's pointer, so it must
 * outlive the frame's walk (a local one was a dangling pointer on the band threads: the TR3 crash) */
enum { ROOF_CACHE = 512, ROOF_SHARE = 50 };
static struct {
    const uint8_t *payload;
    size_t size;
    uint8_t flat[4];
    FshEntry roof;
} roof_cache[ROOF_CACHE];

static uint8_t roof_tone(const TrackArt *art, const FshEntry *t)
{
    size_t count[256] = { 0 }, opaque = 0;
    const size_t pixels = (size_t)t->width * t->height < t->payload_size ? (size_t)t->width * t->height : t->payload_size;
    for (size_t i = 0; i < pixels; i++)
        if (t->payload[i] != 255) count[t->payload[i]]++, opaque++;
    int best = -1, best_luma = 1 << 30;
    for (int c = 0; c < 256; c++) {
        if (count[c] == 0 || count[c] * ROOF_SHARE < opaque) continue;
        const uint8_t *rgb = art->palette + 3 * c;
        const int luma = 299 * rgb[0] + 587 * rgb[1] + 114 * rgb[2];
        if (luma < best_luma) best_luma = luma, best = c;
    }
    return best >= 0 ? (uint8_t)best : 0;
}

static const FshEntry *roof_texture(const TrackArt *art, const FshEntry *t)
{
    for (int i = 0; i < ROOF_CACHE; i++)
        if (roof_cache[i].payload == t->payload && roof_cache[i].size == t->payload_size) return &roof_cache[i].roof;
    static int next;
    const int slot = next++ % ROOF_CACHE;
    roof_cache[slot].payload = t->payload, roof_cache[slot].size = t->payload_size;
    memset(roof_cache[slot].flat, roof_tone(art, t), sizeof roof_cache[slot].flat);
    roof_cache[slot].roof = *t;
    roof_cache[slot].roof.width = 2, roof_cache[slot].roof.height = 2;
    roof_cache[slot].roof.payload = roof_cache[slot].flat, roof_cache[slot].roof.payload_size = sizeof roof_cache[slot].flat;
    return &roof_cache[slot].roof;
}

/* port (track_port_close_boxes): the box's roof, a flat quad of the facade's darkest tone, from camera-space corners */
static void object_roof_camera(void *user, int32_t texture, const int32_t *const cam[4])
{
    const Glue *g = user;
    const FshEntry *t = scenery_texture(g->r->art, texture);
    if (t == NULL || t->payload == NULL) return;
    object_light_on(g);
    full_camera(user, roof_texture(g->r->art, t), cam);
    object_light_off(g);
}

/* port: a scenery board too near for the exe, the whole texture from camera-space corners */
static void object_near_camera(void *user, int32_t texture, const int32_t *const cam[4])
{
    const Glue *g = user;
    const FshEntry *t = scenery_texture(g->r->art, texture);
    if (t == NULL) return;
    int32_t uv[8];
    whole_uv(t, uv);
    object_light_on(g);
    g->ctx->sink.port_near_camera(g->ctx->sink.user, g->ctx->view, t, uv, cam);
    object_light_off(g);
}

void track_render_fence_ops(const TrackGlue *g, TrackFenceOps *out)
{
    *out = (TrackFenceOps){ (void *)g, road_texture, point_screen, subdivide, full_alt,
                            g->ctx->sink.draw_camera != NULL ? full_camera : NULL };
}

void track_render_object_ops(const TrackGlue *g, TrackObjectDrawOps *out)
{
    *out = (TrackObjectDrawOps){ (void *)g, object_model, object_full, object_full_alt,
                                 g->ctx->sink.draw_camera != NULL ? object_full_camera : NULL,
                                 g->ctx->sink.port_near_camera != NULL ? object_near_camera : NULL,
                                 g->ctx->sink.draw_camera != NULL ? object_roof_camera : NULL };
}

void track_render_push_near(int32_t v[3], int32_t near, const int32_t dir[3])
{
    v[0] = clamp_symmetric(v[0], v[2] < near * 2 ? CLAMP_X / 4 : CLAMP_X);
    v[1] = clamp_symmetric(v[1], CLAMP_Y);
    push_to(v, near, dir);
}

void track_render_draw(TrackRender *r, const TrackCamera *cam, const DrawContext *ctx, int mirrored, int32_t frame)
{
    if (r->tri == NULL || r->vertex_count == 0) return;
    const Glue g = { r, cam, ctx, NULL, NULL };
    TrackObjectWalk *walk = &r->build.walk;
    track_objects_from_last(walk);  /* 0044a804, before the draws (0044cc74) */
    TrackObjectDraw objects = { walk, r->nodes, (const int32_t (*)[3])r->object_world,
                                (const int32_t (*)[3])r->object_camera, (const int32_t (*)[2])r->object_screen,
                                TRACK_MAX_OBJECT_VERTICES, frame, r->side_face,
                                { (void *)&g, object_model, object_full, object_full_alt,
                                  ctx->sink.draw_camera != NULL ? object_full_camera : NULL,
                                  ctx->sink.port_near_camera != NULL ? object_near_camera : NULL,
                                  ctx->sink.draw_camera != NULL ? object_roof_camera : NULL } };
    const TrackFenceOps fence_ops = { (void *)&g, road_texture, point_screen, subdivide, full_alt,
                                      ctx->sink.draw_camera != NULL ? full_camera : NULL };
    const TrackFarOps far_ops = { (void *)&g, road_texture, full, cars,
                                  ctx->sink.draw_camera != NULL ? full_camera : NULL };
    const TrackNearOps near_ops = { (void *)&g, skids, no_alternate };
    const TrackFarInputs far = { r->entries, r->build.segment_mask, r->cache, r->vertex_count, mirrored, 0,
                                 { (const int32_t (*)[3])r->world, (const int32_t (*)[3])r->camera,
                                   (const int32_t (*)[2])r->screen, ctx->near, ctx->clip,
                                   r->psx != NULL ? r->light_vertex : NULL, r->light_now, r->psx != NULL ? r->tunnel_vertex : NULL },
                                 &fence_ops, &objects, &r->quarter };
    /* 00449f50 reads the camera's layout; the clip holds its edges */
    const ViewLayout layout = { 0, 0, 0, 0, 0, 0, ctx->clip.x0, ctx->clip.y0, x_sub(ctx->clip.x1, ctx->clip.x0),
                                x_sub(ctx->clip.y1, ctx->clip.y0) };
    const TrackNearInputs near = { &far, &layout, 0, &r->bounds };
    track_near_draw(&far_ops, &near_ops, &near, r->draw_segments, cam->node);
    r->side_face = objects.side_face;
}
