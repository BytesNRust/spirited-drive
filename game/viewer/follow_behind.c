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

#include <stdlib.h>
#include <string.h>

enum {
    WORLD_CAPACITY = TRACK_MAX_VERTICES, OBJECT_CAPACITY = TRACK_MAX_OBJECT_VERTICES, RUN_CAPACITY = 1024,
    ROWS = 4, VARIANTS = 8, MIN_DIR_Z = 0x1999,
    FENCE_RIGHT = 0x80, FENCE_LEFT = 0x40, FENCE_LOW = 0x20, FENCE_TEXTURE = 0x20,
    NODE_POSITION = 8, MIRROR_LOD_DISTANCE = 4
};

typedef struct {
    TrackBuild build;
    int32_t world[WORLD_CAPACITY][3], camera[WORLD_CAPACITY][3], pushed[WORLD_CAPACITY][3], screen[WORLD_CAPACITY][2];
    int32_t object_world[OBJECT_CAPACITY][3], object_camera[OBJECT_CAPACITY][3], object_screen[OBJECT_CAPACITY][2];
    TrackSegmentEntry entries[TRACK_SEGMENT_ENTRIES];
    int32_t run_cursor[RUN_CAPACITY], run_node[RUN_CAPACITY], run_end[RUN_CAPACITY];
    int run_count;
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
} Pass;

static void draw_polygon(Pass *p, const TrackSegmentEntry *e, const uint8_t *record, const TrackPolygon *poly, int near_style,
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
    const int32_t b = band < 0 ? 0 : band;
    const int level = near_style ? (b < TRACK_VARIANT_ROWS ? TRACK_VARIANT_NEAR[b] : 0)
                                 : (b / 4 < TRACK_VARIANT_ROWS ? TRACK_VARIANT_FAR[b / 4] : 0);
    int32_t clipped[4][3];
    int32_t lowest_z = cam[0][2];
    for (int c = 1; c < 4; c++)
        if (cam[c][2] < lowest_z) lowest_z = cam[c][2];
    if (lowest_z < p->ctx.near && p->ctx.sink.draw_camera == NULL) {   /* across the camera: its corners slid to the plane */
        if (!follow_arm_clip_quad(cam, p->ctx.near, clipped)) return;
        for (int c = 0; c < 4; c++) {
            view_project(p->ctx.view, clipped[c], &quad[2 * c]);
            cam[c] = clipped[c];
        }
    }
    if (near_style ? !track_visible_near(&p->bounds, quad, poly->strip, p->arm->mirrored)
                   : !(track_visible_far(&p->ctx.clip, quad, poly->strip, p->arm->mirrored) && cam[3][2] > 0))
        return;
    const FshEntry *t = p->fence_ops.texture(p->fence_ops.ctx, record[2 + poly->strip] * VARIANTS + uv->variant + level);
    if (t == NULL) return;
    if (near_style) track_draw_subdivided(&p->ctx, t, uv->u_level, 0, uv->u_cell, 0, cam, quad, p->ctx.near);
    else if (p->fence_ops.full_camera != NULL) p->fence_ops.full_camera(p->fence_ops.ctx, t, cam);
    else p->ctx.sink.draw_full(p->ctx.sink.user, t, quad);
}

/* one rail panel (0044a2b4's quad: b's top, a's top, a, b) from the points' unpushed camera-space coordinates: clipped to
 * the near plane like the road's quads, its corners slid along the edges, so a rail that passes the lens runs off the
 * screen's edge instead of being dropped or pushed out of shape; one wholly behind the lens is not drawn */
static void draw_rail(Pass *p, const int32_t pair[2], int32_t base, const TrackUv *uv, int level, int32_t texture, int tall)
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
    if (outcode_shared(&p->ctx.clip, screen, 4) != 0) return;
    const FshEntry *t = p->fence_ops.texture(p->fence_ops.ctx, x_add(x_add(uv->variant, level), x_shl(texture, 3)));
    if (t == NULL) return;
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
    const int32_t b = band < 0 ? 0 : band;
    const int level = near_style ? (b < TRACK_VARIANT_ROWS ? TRACK_VARIANT_NEAR[b] : 0)
                                 : (b / 4 < TRACK_VARIANT_ROWS ? TRACK_VARIANT_FAR[b / 4] : 0);
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
        if (p->ctx.sink.draw_cars != NULL) p->ctx.sink.draw_cars(p->ctx.sink.user, (int32_t)((uint32_t)(node + 1) & nm));
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
    const int32_t mask = (int32_t)r->build.segment_mask;
    const int32_t camera_segment = cam->node / 4;
    int32_t first = camera_segment - behind;
    if (!a->circuit && first < 0) first = 0;
    const int32_t last = camera_segment + ahead;
    if (last < first || last + 2 >= TRACK_SEGMENT_ENTRIES) return;
    if (!build_range(r, first, last, camera_segment, a->low_lod)) return;

    Pass *p = &(Pass){ .arm = a, .ctx = *a->ctx, .camera_node = cam->node, .node_mask = (int32_t)r->build.walk.node_mask,
                       .mask = mask, .run_hi = B.run_count - 1 };
    const int shift = p->ctx.world_shift & 31;
    int32_t lo[3], hi[3];
    node_position(r, (camera_segment - 1) * 4, lo);
    node_position(r, camera_segment * 4, hi);
    follow_behind_direction(lo, hi, cam->matrix, shift, p->ctx.near_dir);
    const int32_t near = p->ctx.near;
    const int32_t points = B.build.vertex_count;
    for (int32_t i = 0; i < points; i++) {
        to_camera(cam, shift, B.world[i], B.camera[i]);
        memcpy(B.pushed[i], B.camera[i], sizeof B.pushed[i]);
        track_render_push_near(B.pushed[i], near, p->ctx.near_dir);
        view_project(p->ctx.view, B.pushed[i], B.screen[i]);
    }
    for (int32_t i = 0; i < B.build.walk.vertex_total && i < OBJECT_CAPACITY; i++) {
        to_camera(cam, shift, B.object_world[i], B.object_camera[i]);
        view_project(p->ctx.view, B.object_camera[i], B.object_screen[i]);
    }
    p->glue = (TrackGlue){ r, cam, &p->ctx };
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
            draw_segment(p, lo_s, 1, camera_segment - lo_s < TRACK_NEAR_SEGMENTS);
            lo_s++;
        } else {
            draw_segment(p, hi_s, 0, hi_s - camera_segment < TRACK_NEAR_SEGMENTS);
            hi_s--;
        }
    }
}
