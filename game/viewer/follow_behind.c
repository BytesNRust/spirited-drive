/* The road behind the camera's node (follow_behind.h). */
#include "game/viewer/follow_behind.h"

#include "game/gfx/x86_int.h"
#include "game/race/mirror_scene.h"
#include "game/race/track_build.h"
#include "game/race/track_fence.h"
#include "game/race/track_object_draw.h"
#include "game/race/track_render.h"
#include "game/race/track_tables.h"
#include "game/race/view.h"
#include "game/sys/fixed_math.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    WORLD_CAPACITY = TRACK_MAX_VERTICES, OBJECT_CAPACITY = TRACK_MAX_OBJECT_VERTICES, RUN_CAPACITY = 1024,
    ROWS = 4, VARIANTS = 8, MIN_DIR_Z = 0x1999,
    FENCE_RIGHT = 0x80, FENCE_LEFT = 0x40, FENCE_LOW = 0x20, FENCE_TEXTURE = 0x20,
    NODE_POSITION = 8, MIRROR_LOD_DISTANCE = 4, MIRROR_LEVEL = 1, CUT_GRID = 4, CUT_DIVISOR = 2, CARS_LAG = 5
};

typedef struct {
    TrackBuild build;
    int32_t world[WORLD_CAPACITY][3], camera[WORLD_CAPACITY][3], pushed[WORLD_CAPACITY][3], screen[WORLD_CAPACITY][2];
    int32_t object_world[OBJECT_CAPACITY][3], object_camera[OBJECT_CAPACITY][3], object_screen[OBJECT_CAPACITY][2];
    TrackSegmentEntry entries[TRACK_SEGMENT_ENTRIES];
    int32_t run_cursor[RUN_CAPACITY], run_node[RUN_CAPACITY], run_end[RUN_CAPACITY];
    int run_count;
    /* port (the PS1 light): the light of each point of `world`, 1 for those in a tunnel's row, and the corners of the polygon being drawn, as the render's */
    uint8_t light_vertex[WORLD_CAPACITY], tunnel_vertex[WORLD_CAPACITY], light_now[6];
    int lit;
} Behind;

static Behind B;

static int32_t rd32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

void follow_behind_direction(const int32_t lo[3], const int32_t hi[3], const int32_t matrix[9], int shift, int32_t dir[3])
{
    const int32_t t[3] = { x_sub(lo[0], hi[0]) >> shift, x_sub(lo[1], hi[1]) >> shift, x_sub(lo[2], hi[2]) >> shift };
    mat3_apply(t, matrix, dir);
    if (dir[2] < MIN_DIR_Z) dir[2] = MIN_DIR_Z;
}

static void node_position(const TrackRender *r, int32_t node, int32_t out[3])
{
    const uint8_t *n = r->nodes + ((uint32_t)node & r->build.walk.node_mask) * TRACK_BUILD_NODE_SIZE + NODE_POSITION;
    for (int k = 0; k < 3; k++) out[k] = rd32(n + 4 * k);
}

/* the build of the segments behind: the vertices, and the scenery records on their rows placed as object points, the
 * run of records in node order with where each one's points end */
static int build_range(const TrackRender *r, int32_t first, int32_t last, int32_t camera_segment, int low_lod)
{
    TrackBuild *b = &B.build;
    memset(b, 0, sizeof *b);
    b->cache = r->cache, b->nodes = r->nodes, b->knocked = r->knocked, b->segment_mask = r->build.segment_mask;
    b->world = B.world, b->world_capacity = WORLD_CAPACITY, b->segments = B.entries, b->reach = r->build.reach;
    b->object_world = B.object_world, b->object_capacity = OBJECT_CAPACITY;
    b->walk = r->build.walk;
    B.lit = r->psx != NULL;
    if (B.lit) b->light = r->psx, b->light_out = B.light_vertex, b->tunnel_out = B.tunnel_vertex;   /* the same light, per node, as the main pass's */
    track_objects_start(&b->walk, (int32_t)(b->segment_mask & (uint32_t)first));
    for (int32_t s = first; s <= last; s++)
    {
        /* the level by the distance from the camera, as the exe's from its first; the mirror's no finer than the far level */
        int32_t distance = s < camera_segment ? camera_segment - s : s - camera_segment;
        if (low_lod && distance < MIRROR_LOD_DISTANCE) distance = MIRROR_LOD_DISTANCE;
        track_build_segment(b, s - distance, s, ROWS, 1);
    }
    const TrackObjectWalk *w = &b->walk;
    B.run_count = 0;
    int32_t end = 0;
    for (int32_t i = 0; i < w->count && B.run_count < RUN_CAPACITY && w->record_count > 0; i++) {
        const int32_t at = (w->first + i) % w->record_count;
        const uint8_t *record = w->records + (uint32_t)at * TRACK_WALK_RECORD_SIZE;
        end += w->descriptors[(uint32_t)record[4] * TRACK_WALK_RECORD_SIZE + 1];
        B.run_cursor[B.run_count] = at, B.run_node[B.run_count] = rd32(record), B.run_end[B.run_count++] = end;
    }
    return b->vertex_count <= WORLD_CAPACITY && w->vertex_total <= OBJECT_CAPACITY;
}

static void to_camera(const TrackCamera *cam, int shift, const int32_t world[3], int32_t out[3])
{
    const int32_t t[3] = { x_add(world[0] >> shift, x_sub(0, cam->position[0]) >> shift),
                           x_add(world[1] >> shift, x_sub(0, cam->position[1]) >> shift),
                           x_add(world[2] >> shift, x_sub(0, cam->position[2]) >> shift) };
    mat3_apply(t, cam->matrix, out);
}

typedef struct {
    const FollowArm *arm;
    DrawContext ctx;   /* the main view's, with the road direction reversed */
    TrackGlue glue;
    TrackFenceOps fence_ops;
    TrackObjectDraw objects;
    MirrorBounds bounds;
    int32_t camera_node, node_mask, mask, run_lo, run_hi;
    int backward;   /* the lower nodes lie away from the camera */
    int32_t last_node;   /* the last row drawn nearer by node, whose cars are still to come (-1: none) */
} Pass;

/* the texture's level by the row's band */
static int band_level(int near_style, int32_t band)
{
    const int32_t b = band < 0 ? 0 : band;
    return near_style ? (b < TRACK_VARIANT_ROWS ? TRACK_VARIANT_NEAR[b] : 0) : (b / 4 < TRACK_VARIANT_ROWS ? TRACK_VARIANT_FAR[b / 4] : 0);
}

/* The mirror's rails: the rail texture's edge fringe (palette 187, magenta, next to the keyed texels) is a pixel wide in the
 * exe's pass and a block in the mirror's near panels; a copy of the texture with the fringe keyed out, one per texture */
enum { FRINGE = 187, KEY = 255, MIRROR_TEXTURES = 16, MIRROR_TEXTURE_BYTES = 16384 };

static struct { const FshEntry *source; FshEntry copy; uint8_t data[MIRROR_TEXTURE_BYTES]; } mirror_textures[MIRROR_TEXTURES];

static const FshEntry *mirror_rail_texture(const FshEntry *t)
{
    const size_t bytes = (size_t)t->width * t->height;
    if (t->type != TRACK_TEXTURE_TRANSPARENT_TYPE || bytes > MIRROR_TEXTURE_BYTES || bytes > t->payload_size) return t;
    int free_slot = -1;
    for (int i = 0; i < MIRROR_TEXTURES; i++) {
        if (mirror_textures[i].source == t) return &mirror_textures[i].copy;
        if (mirror_textures[i].source == NULL && free_slot < 0) free_slot = i;
    }
    if (free_slot < 0) return t;
    for (size_t i = 0; i < bytes; i++) mirror_textures[free_slot].data[i] = t->payload[i] == FRINGE ? KEY : t->payload[i];
    mirror_textures[free_slot].source = t;
    mirror_textures[free_slot].copy = *t;
    mirror_textures[free_slot].copy.payload = mirror_textures[free_slot].data;
    return &mirror_textures[free_slot].copy;
}

static int outside_signed(const ClipRect *c, const int32_t s[8])
{
    int left = 1, right = 1, above = 1, below = 1;
    for (int i = 0; i < 4; i++) {
        left &= s[2 * i] < c->x0, right &= s[2 * i] > c->x1;
        above &= s[2 * i + 1] < c->y0, below &= s[2 * i + 1] > c->y1;
    }
    return left || right || above || below;
}

/* the mirror's polygons: a near cell reaches across its box, which the exe's unsigned outcodes (outcode_shared) take for
 * outside on the right when its corners are left of it, so the sides are tested signed, then the facing alone */
static int mirror_visible(const Pass *p, const int32_t s[8], int strip)
{
    if (outside_signed(&p->ctx.clip, s)) return 0;
    const MirrorBounds open = { INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN };
    return track_visible_near(&open, s, strip, p->arm->mirrored);
}

static void lerp3(const int32_t a[3], const int32_t b[3], int32_t f, int32_t out[3])
{
    for (int i = 0; i < 3; i++) out[i] = (int32_t)(a[i] + ((int64_t)b[i] - a[i]) * f / 0x10000);
}

/* A point on the edge a-b, the same whichever end the walk starts from: two neighbours cut by the plane share an edge and walk it
 * from opposite ends, and next to the plane a unit of rounding is tens of pixels, so each reads the edge from its lesser end. */
static int edge_ends_swapped(const int32_t a[3], const int32_t b[3])
{
    for (int i = 2; i >= 0; i--)
        if (a[i] != b[i]) return a[i] > b[i];
    return 0;
}

static void lerp3_edge(const int32_t a[3], const int32_t b[3], int32_t f, int32_t out[3])
{
    if (edge_ends_swapped(a, b)) lerp3(b, a, 0x10000 - f, out);
    else lerp3(a, b, f, out);
}

/* The mirror's polygon across the lens (the classic sink): cut at the near plane as a polygon, the way a neighbour cut by the
 * same plane is, so the two meet on one edge (sliding each quad's corners left a gap between them: the wedge of bare ground),
 * its texture's uv cut with it, drawn as the one to three quads the cut leaves. */
static void draw_cut_cell(const Pass *p, const FshEntry *t, const TrackUv *uv, const int32_t *const cam[4], int strip)
{
    const int32_t w = (int16_t)t->width >> (uv->u_level & 31), h = (int16_t)t->height;
    const int32_t u0 = x_shl(x_mul(x_sub(x_sub(x_shl(1, uv->u_level & 31), 1), uv->u_cell), w), 16);
    const int32_t v0 = x_shl(x_mul(h, x_sub(x_sub(x_shl(1, 0), 1), 0)), 16);
    const int32_t u1 = x_add(u0, x_shl(w - 1, 16)), v1 = x_add(v0, x_shl(h - 1, 16));
    if (!(u0 < u1 && v0 < v1)) return;
    const int32_t plane = p->ctx.near / CUT_DIVISOR > 0 ? p->ctx.near / CUT_DIVISOR : 1;   /* nearer than the near plane: the ground under the lens reaches the screen's bottom */
    const int32_t corner_uv[4][2] = { { u0, v0 }, { u1, v0 }, { u1, v1 }, { u0, v1 } };
    int32_t pos[8][3], tex[8][2];
    int n = 0;
    for (int i = 0; i < 4; i++) {
        const int j = (i + 1) & 3;
        const int in_i = cam[i][2] >= plane, in_j = cam[j][2] >= plane;
        if (in_i) {
            for (int k = 0; k < 3; k++) pos[n][k] = cam[i][k];
            tex[n][0] = corner_uv[i][0], tex[n][1] = corner_uv[i][1], n++;
        }
        if (in_i != in_j) {
            const int lo = edge_ends_swapped(cam[i], cam[j]) ? j : i, hi = lo == i ? j : i;
            const int32_t f = (int32_t)(((int64_t)plane - cam[lo][2]) * 0x10000 / ((int64_t)cam[hi][2] - cam[lo][2]));
            lerp3(cam[lo], cam[hi], f, pos[n]);
            pos[n][2] = plane;
            for (int k = 0; k < 2; k++) tex[n][k] = (int32_t)(corner_uv[lo][k] + ((int64_t)corner_uv[hi][k] - corner_uv[lo][k]) * f / 0x10000);
            n++;
        }
    }
    if (n < 3) return;
    for (int first = 1; first + 1 < n; first += 2) {   /* a fan of quads from vertex 0; a last triangle repeats its corner */
        const int idx[4] = { 0, first, first + 1, first + 2 < n ? first + 2 : first + 1 };
        int32_t grid[CUT_GRID + 1][CUT_GRID + 1][2];
        int32_t grid_uv[CUT_GRID + 1][CUT_GRID + 1][2];
        for (int gy = 0; gy <= CUT_GRID; gy++)
            for (int gx = 0; gx <= CUT_GRID; gx++) {
                const int32_t fx = gx * 0x10000 / CUT_GRID, fy = gy * 0x10000 / CUT_GRID;
                int32_t top[3], bottom[3], point[3], top_uv[2], bottom_uv[2];
                lerp3_edge(pos[idx[0]], pos[idx[1]], fx, top), lerp3_edge(pos[idx[3]], pos[idx[2]], fx, bottom);
                lerp3_edge(top, bottom, fy, point);
                if (point[2] < plane) point[2] = plane;
                for (int k = 0; k < 2; k++) {
                    top_uv[k] = (int32_t)(tex[idx[0]][k] + ((int64_t)tex[idx[1]][k] - tex[idx[0]][k]) * fx / 0x10000);
                    bottom_uv[k] = (int32_t)(tex[idx[3]][k] + ((int64_t)tex[idx[2]][k] - tex[idx[3]][k]) * fx / 0x10000);
                    grid_uv[gy][gx][k] = (int32_t)(top_uv[k] + ((int64_t)bottom_uv[k] - top_uv[k]) * fy / 0x10000);
                }
                view_project(p->ctx.view, point, grid[gy][gx]);
            }
        for (int gy = 0; gy < CUT_GRID; gy++)
            for (int gx = 0; gx < CUT_GRID; gx++) {
                const int32_t q[8] = { grid[gy][gx][0], grid[gy][gx][1], grid[gy][gx + 1][0], grid[gy][gx + 1][1],
                                       grid[gy + 1][gx + 1][0], grid[gy + 1][gx + 1][1], grid[gy + 1][gx][0], grid[gy + 1][gx][1] };
                const int32_t quv[8] = { grid_uv[gy][gx][0], grid_uv[gy][gx][1], grid_uv[gy][gx + 1][0], grid_uv[gy][gx + 1][1],
                                         grid_uv[gy + 1][gx + 1][0], grid_uv[gy + 1][gx + 1][1], grid_uv[gy + 1][gx][0], grid_uv[gy + 1][gx][1] };
                if (!outside_signed(&p->ctx.clip, q)) p->ctx.sink.draw_uv(p->ctx.sink.user, t, quv, q);
            }
    }
    (void)strip;
}

/* port (the PS1 light): a polygon's or rail's corners in light_now (track_fence.h's), and the sink told for the length of the draw as the glue's draws tell it;
 * the draws here that go straight to the sink (the cut cells, the subdivided road) have no glue to do it. Nothing without a light. */
static TrackFenceInputs lit_inputs(void)
{
    return (TrackFenceInputs){ .light = B.lit ? B.light_vertex : NULL, .light_now = B.light_now, .tunnel = B.lit ? B.tunnel_vertex : NULL };
}

static void lit_on(void)
{
    if (track_light_hook != NULL && B.light_now[4] != 0) track_light_tunnel = B.light_now[5], track_light_hook(B.light_now);
}

static void lit_off(void)
{
    if (track_light_hook != NULL && B.light_now[4] != 0) track_light_tunnel = 0, track_light_hook(NULL);
}

static void polygon_body(Pass *p, const TrackSegmentEntry *e, const uint8_t *record, const TrackPolygon *poly, int near_style, int32_t band);

static void draw_polygon(Pass *p, const TrackSegmentEntry *e, const uint8_t *record, const TrackPolygon *poly, int near_style,
                         int32_t band)
{
    const TrackFenceInputs in = lit_inputs();
    track_light_set(&in, e->first, poly->corner);
    lit_on();
    polygon_body(p, e, record, poly, near_style, band);
    lit_off();
    track_light_clear(&in);
}

static void polygon_body(Pass *p, const TrackSegmentEntry *e, const uint8_t *record, const TrackPolygon *poly, int near_style,
                         int32_t band)
{
    const TrackUv *uv = &TRACK_UVS[poly->uv];
    int32_t quad[8];
    const int32_t *cam[4];
    for (int c = 0; c < 4; c++) {
        const int32_t v = e->first + poly->corner[c];
        quad[2 * c] = B.screen[v][0], quad[2 * c + 1] = B.screen[v][1];
        cam[c] = B.camera[v];
    }
    const int level = p->arm->low_lod ? MIRROR_LEVEL : band_level(near_style, band);   /* the mirror's roads at the one level it draws them at (004c2e94) */
    int32_t clipped[4][3];
    int32_t lowest_z = cam[0][2];
    for (int c = 1; c < 4; c++)
        if (cam[c][2] < lowest_z) lowest_z = cam[c][2];
    const int cut = lowest_z < p->ctx.near && p->ctx.sink.draw_camera == NULL;
    if (cut && near_style) {   /* the polygon cut at the plane */
        const FshEntry *whole = p->fence_ops.texture(p->fence_ops.ctx, record[2 + poly->strip] * VARIANTS + uv->variant + level);
        if (whole != NULL) draw_cut_cell(p, whole, uv, cam, poly->strip);
        return;
    }
    if (cut) {   /* across the camera: its corners slid to the plane */
        if (!follow_arm_clip_quad(cam, p->ctx.near, clipped)) return;
        for (int c = 0; c < 4; c++) {
            view_project(p->ctx.view, clipped[c], &quad[2 * c]);
            cam[c] = clipped[c];
        }
    }
    if (p->arm->low_lod ? !mirror_visible(p, quad, poly->strip)
        : near_style ? !track_visible_near(&p->bounds, quad, poly->strip, p->arm->mirrored)
                     : !(track_visible_far(&p->ctx.clip, quad, poly->strip, p->arm->mirrored) && cam[3][2] > 0))
        return;
    const FshEntry *t = p->fence_ops.texture(p->fence_ops.ctx, record[2 + poly->strip] * VARIANTS + uv->variant + level);
    if (t == NULL) return;
    if (near_style) track_draw_subdivided(&p->ctx, t, uv->u_level, 0, uv->u_cell, 0, cam, quad, p->ctx.near);
    else if (p->fence_ops.full_camera != NULL) p->fence_ops.full_camera(p->fence_ops.ctx, t, cam);
    else p->ctx.sink.draw_full(p->ctx.sink.user, t, quad);
}

/* the part of an edge from z0 to z1 in front of the plane as 16.16 fractions of it; 0 when none is */
static int edge_in_front(int32_t z0, int32_t z1, int32_t near, int32_t out[2])
{
    out[0] = 0, out[1] = 0x10000;
    if (z0 >= near && z1 >= near) return 1;
    if (z0 < near && z1 < near) return 0;
    const int64_t span = (int64_t)z1 - z0;
    const int32_t cut = (int32_t)(((int64_t)near - z0) * 0x10000 / span);
    if (z0 < near) out[0] = cut;
    else out[1] = cut;
    return 1;
}

/* a whole-texture rail panel across the near plane (the classic sink): both edges cut at the plane, the texture's u cut
 * with them, so the panel's end is the part of the rail that is in front and not the whole texture squeezed into it */
static void draw_rail_cut(const Pass *p, const FshEntry *t, const int32_t *const c[4])
{
    int32_t top[2], bottom[2];
    if (!edge_in_front(c[0][2], c[1][2], p->ctx.near, top) || !edge_in_front(c[3][2], c[2][2], p->ctx.near, bottom)) return;
    int32_t n[4][3], screen[8];
    lerp3(c[0], c[1], top[0], n[0]), lerp3(c[0], c[1], top[1], n[1]);
    lerp3(c[3], c[2], bottom[1], n[2]), lerp3(c[3], c[2], bottom[0], n[3]);
    for (int i = 0; i < 4; i++) {
        if (n[i][2] < p->ctx.near) n[i][2] = p->ctx.near;
        view_project(p->ctx.view, n[i], &screen[2 * i]);
    }
    if (outside_signed(&p->ctx.clip, screen)) return;
    const int32_t right = x_sub(x_shl((int16_t)t->width, 16), 1), bottom_v = x_sub(x_shl((int16_t)t->height, 16), 1);
    const int32_t u[4] = { (int32_t)((int64_t)right * top[0] >> 16), (int32_t)((int64_t)right * top[1] >> 16),
                           (int32_t)((int64_t)right * bottom[1] >> 16), (int32_t)((int64_t)right * bottom[0] >> 16) };
    const int32_t uvs[8] = { u[0], 0, u[1], 0, u[2], bottom_v, u[3], bottom_v };
    p->ctx.sink.draw_uv(p->ctx.sink.user, t, uvs, screen);
}

/* one rail panel (0044a2b4's quad: b's top, a's top, a, b) from the points' unpushed camera-space coordinates: clipped to
 * the near plane like the road's quads, its corners slid along the edges, so a rail that passes the lens runs off the
 * screen's edge instead of being dropped or pushed out of shape; one wholly behind the lens is not drawn */
static void rail_body(Pass *p, const int32_t pair[2], int32_t base, const TrackUv *uv, int level, int32_t texture, int tall);

static void draw_rail(Pass *p, const int32_t pair[2], int32_t base, const TrackUv *uv, int level, int32_t texture, int tall)
{
    const TrackFenceInputs in = lit_inputs();
    track_light_set_points(&in, base + pair[0], base + pair[1]);
    lit_on();
    rail_body(p, pair, base, uv, level, texture, tall);
    lit_off();
    track_light_clear(&in);
}

static void rail_body(Pass *p, const int32_t pair[2], int32_t base, const TrackUv *uv, int level, int32_t texture, int tall)
{
    const int32_t height = tall ? 0x20000 : 0xE666;
    const int32_t a = base + pair[0], b = base + pair[1];
    int32_t tcam[2][3];
    const int32_t ends[2] = { b, a };
    for (int k = 0; k < 2; k++) {
        const int32_t up[3] = { B.world[ends[k]][0], x_add(B.world[ends[k]][1], height), B.world[ends[k]][2] };
        to_camera(p->arm->camera, p->ctx.world_shift & 31, up, tcam[k]);
    }
    const int32_t *corners[4] = { tcam[0], tcam[1], B.camera[a], B.camera[b] };
    int32_t clipped[4][3], screen[8];
    int32_t lowest = corners[0][2];
    for (int c = 1; c < 4; c++)
        if (corners[c][2] < lowest) lowest = corners[c][2];
    if (lowest < p->ctx.near && p->ctx.sink.draw_camera == NULL && uv->variant != 0) {
        const FshEntry *whole = p->fence_ops.texture(p->fence_ops.ctx, x_add(x_add(uv->variant, level), x_shl(texture, 3)));
        if (whole != NULL) draw_rail_cut(p, p->arm->low_lod ? mirror_rail_texture(whole) : whole, corners);
        return;
    }
    if (lowest < p->ctx.near) {
        if (p->ctx.sink.draw_camera != NULL) {   /* the sink clips the cell itself; wholly behind the lens: nothing */
            int32_t highest = corners[0][2];
            for (int c = 1; c < 4; c++)
                if (corners[c][2] > highest) highest = corners[c][2];
            if (highest <= 0) return;
        } else {
            if (!follow_arm_clip_quad(corners, p->ctx.near, clipped)) return;
            for (int c = 0; c < 4; c++) corners[c] = clipped[c];
        }
    }
    for (int c = 0; c < 4; c++) {
        int32_t pushed[3] = { corners[c][0], corners[c][1], corners[c][2] };
        track_render_push_near(pushed, p->ctx.near, p->ctx.near_dir);
        view_project(p->ctx.view, pushed, &screen[2 * c]);
    }
    if (outside_signed(&p->ctx.clip, screen)) return;
    const FshEntry *t = p->fence_ops.texture(p->fence_ops.ctx, x_add(x_add(uv->variant, level), x_shl(texture, 3)));
    if (t == NULL) return;
    if (p->arm->low_lod) t = mirror_rail_texture(t);
    if (uv->variant == 0) p->fence_ops.subdivide(p->fence_ops.ctx, t, uv->u_level, uv->u_cell, corners, screen, p->ctx.near);
    else if (p->fence_ops.full_camera != NULL) p->fence_ops.full_camera(p->fence_ops.ctx, t, corners);
    else p->fence_ops.full_alt(p->fence_ops.ctx, t, screen);
}

static void draw_fences(Pass *p, const TrackLod *lod, const uint8_t *record, const TrackUv *uv, int32_t base, int32_t at,
                        int level)
{
    const uint8_t fence = record[1];
    const int32_t texture = (fence & 0x1f) + FENCE_TEXTURE;
    const int tall = (fence & FENCE_LOW) == 0;
    const int pair = (at & 3) * 2;
    if (fence & FENCE_RIGHT) draw_rail(p, (const int32_t[2]){ lod->fence[8 + pair], lod->fence[8 + pair + 1] }, base, uv, level, texture, tall);
    if (fence & FENCE_LEFT) draw_rail(p, (const int32_t[2]){ lod->fence[pair], lod->fence[pair + 1] }, base, uv, level, texture, tall);
}

/* the scenery on a row's node, the walker put on each record in turn: up the run behind the camera, down it ahead */
static void draw_objects(Pass *p, int32_t node, int32_t band, int ascending)
{
    TrackObjectWalk *w = p->objects.walk;
    if (ascending) {
        while (p->run_lo < B.run_count && B.run_node[p->run_lo] < node) p->run_lo++;
        while (p->run_lo < B.run_count && B.run_node[p->run_lo] == node) {
            w->cursor = B.run_cursor[p->run_lo];
            w->node = node;
            w->vertex_cursor = B.run_end[p->run_lo++];
            track_object_draw(&p->objects, band, 1);
        }
    } else {
        while (p->run_hi >= 0 && B.run_node[p->run_hi] > node) p->run_hi--;
        while (p->run_hi >= 0 && B.run_node[p->run_hi] == node) {
            w->cursor = B.run_cursor[p->run_hi];
            w->node = node;
            w->vertex_cursor = B.run_end[p->run_hi--];
            track_object_draw(&p->objects, band, 1);
        }
    }
}

/* a block of a segment's polygons starts with the one that closes `rows` rows (the exe draws the polygons last to first
 * and puts the rows' rail and scenery up when it reaches that one): they are drawn, then the rows' rail and what stands
 * on them (the cars between the rows), the farthest row first: the lowest node behind the camera, the highest ahead of it */
static void draw_block(Pass *p, const TrackLod *lod, const TrackSegmentEntry *e, const uint8_t *record, int32_t segment,
                       int32_t row, int32_t rows, int32_t from, int32_t to, int near_style, int ascending)
{
    const TrackPolygon *marker = &TRACK_POLYGONS[lod->first_polygon + from];
    const uint32_t nm = (uint32_t)p->node_mask;
    const int32_t lowest = (int32_t)((uint32_t)(segment * 4 + row) & nm);
    const int32_t highest = (int32_t)((uint32_t)(segment * 4 + row + rows - 1) & nm);
    const int32_t band = (ascending ? p->camera_node - lowest : highest - p->camera_node) + 4;
    for (int32_t i = to; i >= from; i--) draw_polygon(p, e, record, &TRACK_POLYGONS[lod->first_polygon + i], near_style, band);
    if (rows == 0) return;
    const TrackUv *uv = &TRACK_UVS[marker->uv];
    const int level = band_level(near_style, band);
    draw_fences(p, lod, record, uv, e->first, highest, level);
    int32_t quad[8];
    for (int c = 0; c < 4; c++) {
        const int32_t v = e->first + marker->corner[c];
        quad[2 * c] = B.screen[v][0], quad[2 * c + 1] = B.screen[v][1];
    }
    for (int32_t q = 0; q < rows; q++) {
        const int32_t node = (int32_t)((uint32_t)(ascending ? lowest + q : highest - q) & nm);
        draw_objects(p, node, band, ascending);
        if (near_style && p->ctx.sink.draw_skids != NULL) p->ctx.sink.draw_skids(p->ctx.sink.user, node, &quad[0], &quad[6]);
        if (p->ctx.sink.draw_cars == NULL) continue;
        if (ascending) {   /* nearer rows follow: a row's cars after the next rows' polygons, as the exe mirror's (0045a8fc) */
            p->ctx.sink.draw_cars(p->ctx.sink.user, (int32_t)((uint32_t)(node + 1 - CARS_LAG) & nm));
            p->last_node = node;
        } else {
            p->ctx.sink.draw_cars(p->ctx.sink.user, (int32_t)((uint32_t)(node + 1) & nm));
        }
    }
}

/* one segment's polygons in their blocks: the lowest node's block first behind the camera, the highest's first from it on,
 * as the exe's */
static void draw_segment(Pass *p, int32_t s, int ascending, int near_style)
{
    enum { MAX_BLOCKS = 9 };
    const TrackRender *r = p->arm->render;
    const TrackSegmentEntry *e = &B.entries[(uint32_t)s & (uint32_t)p->mask];
    const TrackLod *lod = &TRACK_LODS[e->lod < TRACK_LOD_COUNT ? e->lod : 0];
    const uint8_t *record = r->cache + mirror_segment_offset(s, p->mask);
    const int32_t count = lod->polygon_count;
    if (count <= 0 || lod->first_polygon + count > TRACK_POLYGON_COUNT) return;
    int32_t start[MAX_BLOCKS], rows[MAX_BLOCKS], blocks = 0;
    if (TRACK_POLYGONS[lod->first_polygon].rows_done == 0) start[blocks] = 0, rows[blocks++] = 0;   /* before the first marker */
    for (int32_t i = 0; i < count && blocks < MAX_BLOCKS; i++)
        if (TRACK_POLYGONS[lod->first_polygon + i].rows_done > 0) start[blocks] = i, rows[blocks++] = TRACK_POLYGONS[lod->first_polygon + i].rows_done;
    int32_t first_row[MAX_BLOCKS], row = 0;
    for (int32_t j = 0; j < blocks; j++) first_row[j] = row, row += rows[j];
    for (int32_t k = 0; k < blocks; k++) {
        const int32_t j = ascending ? k : blocks - 1 - k;
        const int32_t end = j + 1 < blocks ? start[j + 1] - 1 : count - 1;
        draw_block(p, lod, e, record, s, first_row[j], rows[j], start[j], end, near_style, ascending);
    }
}

void follow_behind_draw(const FollowArm *a, int32_t behind, int32_t ahead)
{
    const TrackRender *r = a->render;
    const TrackCamera *cam = a->camera;
    if (r->tri == NULL) return;
    if (behind > FOLLOW_ARM_SEGMENTS) behind = FOLLOW_ARM_SEGMENTS;
    behind = follow_arm_kept_behind(&r->window, behind);   /* no farther than the cache keeps: past it the records are the far ahead's (Ultra) */
    const int32_t mask = (int32_t)r->build.segment_mask;
    const int32_t camera_segment = cam->node / 4;
    int32_t first = camera_segment - behind;
    if (!a->circuit && first < 0) first = 0;
    const int32_t last = camera_segment + ahead;
    if (last < first || last + 3 >= TRACK_SEGMENT_ENTRIES) return;
    if (!build_range(r, first, last + 1, camera_segment, a->low_lod)) return;   /* the segment past the last: its first row closes the last's polygons */

    Pass *p = &(Pass){ .arm = a, .ctx = *a->ctx, .camera_node = cam->node, .node_mask = (int32_t)r->build.walk.node_mask,
                       .mask = mask, .run_hi = B.run_count - 1, .last_node = -1 };
    const int shift = p->ctx.world_shift & 31;
    int32_t lo[3], hi[3];
    node_position(r, (camera_segment - 1) * 4, lo);
    node_position(r, camera_segment * 4, hi);
    follow_behind_direction(lo, hi, cam->matrix, shift, p->ctx.near_dir);
    {
        const int32_t toward_lower[3] = { x_sub(lo[0], hi[0]) >> shift, x_sub(lo[1], hi[1]) >> shift, x_sub(lo[2], hi[2]) >> shift };
        int32_t raw[3];
        mat3_apply(toward_lower, cam->matrix, raw);
        p->backward = raw[2] > 0;
    }
    const int32_t points = B.build.vertex_count;
    for (int32_t i = 0; i < points; i++) {
        to_camera(cam, shift, B.world[i], B.camera[i]);
        memcpy(B.pushed[i], B.camera[i], sizeof B.pushed[i]);
        track_render_push_near(B.pushed[i], p->ctx.near, p->ctx.near_dir);
        view_project(p->ctx.view, B.pushed[i], B.screen[i]);
    }
    for (int32_t i = 0; i < B.build.walk.vertex_total && i < OBJECT_CAPACITY; i++) {
        to_camera(cam, shift, B.object_world[i], B.object_camera[i]);
        view_project(p->ctx.view, B.object_camera[i], B.object_screen[i]);
    }
    p->glue = (TrackGlue){ r, cam, &p->ctx, B.light_now, &B.build.walk };
    track_render_fence_ops(&p->glue, &p->fence_ops);
    TrackObjectDrawOps object_ops;
    track_render_object_ops(&p->glue, &object_ops);
    p->objects = (TrackObjectDraw){ &B.build.walk, r->nodes, (const int32_t (*)[3])B.object_world,
                                    (const int32_t (*)[3])B.object_camera, (const int32_t (*)[2])B.object_screen,
                                    OBJECT_CAPACITY, a->frame, 0, object_ops };
    /* the exe drops a near polygon whose points 0 and 3 are below the box, as it never draws one that reaches past it on a road
     * ahead; with the road pushed along its other direction one can span the screen, so no bottom: the sink clips */
    p->bounds = (MirrorBounds){ 0x7fffffff, p->ctx.clip.x1, p->ctx.clip.x0, p->ctx.clip.y0 };
    /* the farthest segment first: from either end in, the one farther from the camera's */
    int32_t lo_s = first, hi_s = last;
    while (lo_s < camera_segment || hi_s >= camera_segment) {
        const int take_lo = lo_s < camera_segment && (hi_s < camera_segment || camera_segment - lo_s > hi_s - camera_segment);
        if (take_lo) {
            draw_segment(p, lo_s, 1, a->low_lod || camera_segment - lo_s < TRACK_NEAR_SEGMENTS);
            lo_s++;
        } else {
            /* the camera's own segment, seen from a camera that looks back along the road: its lowest node is the farthest */
            draw_segment(p, hi_s, hi_s == camera_segment && p->backward, a->low_lod || hi_s - camera_segment < TRACK_NEAR_SEGMENTS);
            hi_s--;
        }
    }
    if (p->last_node >= 0 && p->ctx.sink.draw_cars != NULL)   /* the rows the lag left: the nearest cars */
        for (int32_t k = 1; k < CARS_LAG; k++)
            p->ctx.sink.draw_cars(p->ctx.sink.user, (int32_t)((uint32_t)(p->last_node + 1 - CARS_LAG + k) & (uint32_t)p->node_mask));
}
