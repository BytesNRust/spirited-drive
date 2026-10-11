#include "game/race/track_render.h"

#include "game/race/track_geometry.h"
#include "game/race/track_tables.h"
#include "game/sys/fixed_math.h"

#include <string.h>

enum {
    ROW_POINTS = TRACK_ROW_POINTS,
    NEAR_PLANE = 62259,      /* 004c52c0, world 16.16 */
    CLAMP_X = 0xA80000,      /* 004c52c8 */
    CLAMP_Y = 5376,          /* 004c52cc */
    MIN_DIR_Z = 0x1999       /* 004497d8 */
};

/* ---- camera matrix (004496ac) ---- */

void track_camera_matrix(int32_t pitch, int32_t yaw, int32_t roll, int mirrored, int32_t out[9])
{
    int32_t x[9], y[9], z[9], t[9], u[9];
    mat3_rotate_x(x, pitch);
    mat3_rotate_z(z, roll);
    mat3_rotate_y(y, yaw);
    /* TODO(00449670): the viewport's vertical scale; 0x10000 in every view
     * seen so far. */
    const int32_t flip[9] = { mirrored ? -0x10000 : 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };
    mat3_mul(y, z, t);
    mat3_mul(t, x, u);
    mat3_mul(u, flip, out);
}

/* ---- vertex list (0043f48c, 0043eca4) ---- */

static TrackSegmentRecord *record(TrackRender *r, int32_t segment)
{
    const int32_t i = segment - r->first_segment;
    return i >= 0 && i < TRACK_WINDOW ? &r->segments[i] : NULL;
}

static const TrackSegmentRecord *crecord(const TrackRender *r, int32_t segment)
{
    const int32_t i = segment - r->first_segment;
    return i >= 0 && i < TRACK_WINDOW ? &r->segments[i] : NULL;
}

static int property_of(const Tri *tri, int32_t segment)
{
    TriNode n;
    if (tri_node(tri, ((uint32_t)segment * TRI_NODES_PER_SEGMENT) & (tri->ring_nodes - 1), &n) != TRI_OK) return 0;
    return n.property < TRACK_PROPERTIES ? n.property : 0;
}

/* Scenery records are sorted by node; the cursor walks them in step with
 * the rows being built (005041c4 / 005041c8). */
typedef struct {
    uint32_t next;
    int done;
} ObjectCursor;

/* 0044a9b8: the first record at or after a node. */
static ObjectCursor objects_from(const Tri *tri, int32_t node)
{
    ObjectCursor c = { 0, tri->scenery_count == 0 };
    TriScenery s;
    while (c.next < tri->scenery_count && tri_scenery(tri, c.next, &s) == TRI_OK && s.node < node) c.next++;
    if (c.next >= tri->scenery_count) {
        c.next = 0;
        c.done = !tri_is_circuit(tri);
    }
    return c;
}

/* 0043eca4, scenery part: every record on this node, placed. */
static void add_objects(TrackRender *r, ObjectCursor *c, int32_t node)
{
    TriScenery s;
    uint32_t guard = 0;
    while (!c->done && guard++ < r->tri->scenery_count && tri_scenery(r->tri, c->next, &s) == TRI_OK
           && s.node == node) {
        TrackRenderObject *o = r->object_count < TRACK_MAX_OBJECTS ? &r->objects[r->object_count] : NULL;
        if (o != NULL && track_object(r->tri, c->next, &o->placed) == TRI_OK) {
            const int corners = o->placed.descriptor.type == TRACK_OBJECT_MODEL ? 1 : o->placed.corner_count;
            if (r->object_vertex_count + corners <= TRACK_MAX_OBJECT_VERTICES) {
                o->node = node;
                o->record = c->next;
                o->descriptor = o->placed.descriptor;
                o->record_flag = s.flags[2];
                o->first = r->object_vertex_count;
                o->count = corners;
                for (int k = 0; k < corners; k++) {
                    const TrackPoint p = corners == 1 && o->placed.corner_count == 0 ? o->placed.origin
                                                                                     : o->placed.corners[k];
                    r->object_world[r->object_vertex_count][0] = p.x;
                    r->object_world[r->object_vertex_count][1] = p.y;
                    r->object_world[r->object_vertex_count][2] = p.z;
                    r->object_vertex_count++;
                }
                r->object_count++;
            }
        }
        if (++c->next >= r->tri->scenery_count) {
            c->next = 0;
            c->done = !tri_is_circuit(r->tri);
        }
    }
}

/* 0043eca4, terrain part: the LOD's selected points of the first `rows`
 * rows, appended to the vertex list; with a cursor, the scenery of each
 * row's node too. */
static void add_segment(TrackRender *r, int32_t segment, int rows, ObjectCursor *objects)
{
    TrackSegmentRecord *rec = record(r, segment);
    TrackSegment seg;
    if (rec == NULL) return;
    const int32_t distance = segment - r->first_segment;
    rec->lod = TRACK_LOD_BY_DISTANCE[property_of(r->tri, segment)][distance < TRACK_LOD_DISTANCES ? distance : 0];
    rec->first = r->vertex_count;
    rec->count = 0;
    rec->valid = (segment >= 0 || tri_is_circuit(r->tri))
        && track_decode_segment(r->tri, (uint32_t)segment, &seg) == TRI_OK;
    if (!rec->valid) {
        /* Past the end of a stage: placeholders keep the vertex indices of
         * the LOD tables lined up; nothing touching them is drawn. */
        memset(&seg, 0, sizeof seg);
        if (r->vertex_count > 0)
            for (int row = 0; row < TRACK_SEGMENT_ROWS; row++)
                for (int point = 0; point < ROW_POINTS; point++)
                    seg.rows[row][point] = (TrackPoint){ r->world[r->vertex_count - 1][0],
                                                         r->world[r->vertex_count - 1][1],
                                                         r->world[r->vertex_count - 1][2] };
    }
    memcpy(rec->texture, seg.texture, sizeof rec->texture);
    rec->fence = seg.fence;
    const uint8_t *select = TRACK_LODS[rec->lod].select;
    for (int row = 0; row < rows; row++) {
        for (int point = 0; *select != 0xFF && point < ROW_POINTS; point++) {
            if (*select != point) continue;
            if (r->vertex_count < TRACK_MAX_VERTICES) {
                const TrackPoint p = seg.rows[row][point];
                r->world[r->vertex_count][0] = p.x;
                r->world[r->vertex_count][1] = p.y;
                r->world[r->vertex_count][2] = p.z;
                r->vertex_count++;
            }
            select++;
        }
        while (*select != 0xFF) select++;  /* the original stops at 0xFF */
        select++;
        if (objects != NULL)
            add_objects(r, objects, ((segment * TRI_NODES_PER_SEGMENT) + row) & (int32_t)(r->tri->ring_nodes - 1));
    }
    rec->count = r->vertex_count - rec->first;
}

void track_render_build(TrackRender *r, const Tri *tri, const TrackArt *art, int32_t camera_node)
{
    memset(r->segments, 0, sizeof r->segments);
    r->tri = tri;
    r->art = art;
    r->vertex_count = 0;
    r->object_count = 0;
    r->object_vertex_count = 0;
    r->first_segment = camera_node / 4;  /* rounds toward zero, as the original */
    int32_t s = r->first_segment;
    ObjectCursor objects = objects_from(tri, (s * TRI_NODES_PER_SEGMENT) & (int32_t)(tri->ring_nodes - 1));
    for (int i = 0; i < TRACK_DRAW_SEGMENTS - 1; i++, s++) add_segment(r, s, 4, &objects);
    add_segment(r, s, 4, NULL);
    add_segment(r, s + 1, 1, NULL);
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
    const int32_t gap = limit - v[2];
    const int32_t t = (int32_t)(((int64_t)(gap % dir[2]) << 16) / dir[2]) + (int32_t)((uint32_t)(gap / dir[2]) << 16);
    v[0] += mul_round(t, dir[0]);
    v[1] += mul_round(t, dir[1]);
    v[2] = limit;
}

static int32_t clamp_symmetric(int32_t v, int32_t limit)
{
    if (limit < v) return limit;
    if (v < -limit) return -limit;
    return v;
}

void track_render_transform(TrackRender *r, const TrackCamera *cam, DrawContext *ctx)
{
    const int shift = ctx->world_shift;
    const int32_t offset[3] = { -cam->position[0] >> shift, -cam->position[1] >> shift, -cam->position[2] >> shift };
    for (int i = 0; i < r->vertex_count; i++) {
        const int32_t t[3] = { (r->world[i][0] >> shift) + offset[0], (r->world[i][1] >> shift) + offset[1],
                               (r->world[i][2] >> shift) + offset[2] };
        mat3_apply(t, cam->matrix, r->camera[i]);
    }
    /* 004497d8: the road direction across the camera's segment. */
    const TrackSegmentRecord *first = crecord(r, cam->node / 4);
    const int next = first != NULL && first->count < r->vertex_count ? first->count : 0;
    for (int k = 0; k < 3; k++) ctx->near_dir[k] = r->camera[next][k] - r->camera[0][k];
    if ((ctx->near_dir[2] < 0 ? -ctx->near_dir[2] : ctx->near_dir[2]) < MIN_DIR_Z) ctx->near_dir[2] = MIN_DIR_Z;

    const int32_t near = NEAR_PLANE >> shift;
    const int near_count = r->vertex_count - (TRACK_DRAW_SEGMENTS - 3) * ROW_POINTS;
    for (int i = 0; i < r->vertex_count; i++) {
        int32_t *v = r->camera[i];
        if (i < near_count) {
            v[0] = clamp_symmetric(v[0], v[2] < near * 2 ? CLAMP_X / 4 : CLAMP_X);
            v[1] = clamp_symmetric(v[1], CLAMP_Y);
            push_to(v, near, ctx->near_dir);
        } else {
            push_to(v, near * 8, ctx->near_dir);
        }
    }
    for (int i = 0; i < r->vertex_count; i++) view_project(ctx->view, r->camera[i], r->screen[i]);
    for (int i = 0; i < r->object_vertex_count; i++) {
        const int32_t t[3] = { (r->object_world[i][0] >> shift) + offset[0], (r->object_world[i][1] >> shift) + offset[1],
                               (r->object_world[i][2] >> shift) + offset[2] };
        mat3_apply(t, cam->matrix, r->object_camera[i]);
        view_project(ctx->view, r->object_camera[i], r->object_screen[i]);
    }
}

/* ---- visibility (00449f90, 0044a0d0) ---- */

static const int TRIANGLES[3][3] = { { 1, 2, 3 }, { 3, 0, 1 }, { 2, 3, 0 } };  /* 004c5320.. */

/* 00449f0c / 0048f750: twice the signed area of a screen triangle, reversed
 * on mirrored tracks. 32-bit wrap as in the original. */
static int32_t winding(const int32_t *s, const int *tri, int mirrored)
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
        const int32_t w = winding(s, TRIANGLES[t], mirrored);
        if (strip < 5 ? w < 0 : w >= 1) return 1;
    }
    return 0;
}

/* 00449f90 */
static int visible_near(const ClipRect *clip, const int32_t *s, int strip, int mirrored)
{
    const int right = s[0] > clip->x1 && s[2] > clip->x1 && s[4] > clip->x1 && s[6] > clip->x1;
    const int left = s[0] < clip->x0 && s[2] < clip->x0 && s[4] < clip->x0 && s[6] < clip->x0;
    const int below = s[1] > clip->y1 && s[7] > clip->y1;
    if (right || left || below) return 0;
    return faces_camera(s, strip, 3, mirrored);
}

/* 0044a0d0 */
static int visible_far(const ClipRect *clip, const int32_t *s, int strip, int mirrored)
{
    if (outcode_shared(clip, s, 4) != 0) return 0;
    return faces_camera(s, strip, 2, mirrored);
}

/* ---- drawing (0044ad5c, 0044b510) ---- */

/* The slot table is read flat: texture * 8 + variant may run into the next
 * texture's variants, as in the original. */
static const FshEntry *texture_of(const TrackArt *art, int texture, int variant)
{
    const int flat = texture * TRACK_TEXTURE_VARIANTS + variant;
    if (flat < 0 || flat >= TRACK_TEXTURE_SLOTS * TRACK_TEXTURE_VARIANTS) return NULL;
    const TrackTexture *t = &art->slots[flat / TRACK_TEXTURE_VARIANTS][flat % TRACK_TEXTURE_VARIANTS];
    return t->present ? &t->bitmap : NULL;
}

static void gather(const TrackRender *r, int base, const TrackPolygon *p, int32_t screen[8], const int32_t *cam[4])
{
    for (int i = 0; i < 4; i++) {
        const int v = base + p->corner[i];
        const int ok = v >= 0 && v < r->vertex_count;
        screen[2 * i] = ok ? r->screen[v][0] : INT32_MIN;
        screen[2 * i + 1] = ok ? r->screen[v][1] : INT32_MIN;
        cam[i] = ok ? r->camera[v] : r->camera[0];
    }
}

static int variant_far(int row)
{
    const int i = row / 4;
    return i >= 0 && i < TRACK_VARIANT_ROWS ? TRACK_VARIANT_FAR[i] : 0;
}

static int variant_near(int row)
{
    return row >= 0 && row < TRACK_VARIANT_ROWS ? TRACK_VARIANT_NEAR[row] : 0;
}

/* ---- scenery (0044a4b0) ---- */

enum { MIN_OBJECT_Z = 0x86, SCREEN_LIMIT = 0x801 };

/* Walks the rows from far to near; objects are drawn when their row is
 * passed (0044b510 / 0044ad5c, local node counter). */
typedef struct {
    int32_t node;
    int cursor;     /* next object to draw, from the end */
    int32_t frame;
} RowWalker;

/* 0044a450: every corner within 0x800 of the origin horizontally. */
static int on_screen(const int32_t *s)
{
    for (int i = 0; i < 4; i++)
        if (!(s[2 * i] < SCREEN_LIMIT && -SCREEN_LIMIT < s[2 * i])) return 0;
    return 1;
}

static const FshEntry *scenery_texture(const TrackArt *art, int resource)
{
    /* 005100c4 holds 4 dwords per texture; the distance pick within them
     * (004c2da4 / 004c2e1c) lands on copies of the same bitmap. */
    const int id = resource / 4;
    return id >= 0 && id < TRACK_SCENERY_TEXTURES && art->scenery[id].present ? &art->scenery[id].bitmap : NULL;
}

static void corners_of(const TrackRender *r, const TrackRenderObject *o, const int order[4], int32_t out[8])
{
    for (int i = 0; i < 4; i++) {
        out[2 * i] = r->object_screen[o->first + order[i]][0];
        out[2 * i + 1] = r->object_screen[o->first + order[i]][1];
    }
}

static int in_front(const TrackRender *r, const TrackRenderObject *o, int from, int count)
{
    for (int i = from; i < from + count; i++)
        if (r->object_camera[o->first + i][2] < MIN_OBJECT_Z) return 0;
    return 1;
}

static void draw_object(const TrackRender *r, const DrawContext *ctx, const TrackRenderObject *o, int32_t frame)
{
    const TriDescriptor *d = &o->descriptor;
    if (d->type == TRACK_OBJECT_MODEL) {
        if (ctx->sink.draw_model != NULL) {
            const int32_t p[3] = { o->placed.origin.x, o->placed.origin.y, o->placed.origin.z };
            ctx->sink.draw_model(ctx->sink.user, d->resource_id, p, o->placed.heading);
        }
        return;
    }
    if (!in_front(r, o, 0, 4)) return;
    int32_t screen[8];
    if (d->type == TRACK_OBJECT_BILLBOARD) {
        int resource = d->resource_id;
        if ((d->flags & 4) != 0) {  /* animated: frames (+8) at a rate (+9) */
            const int frames = d->unknown_08 & 0xFF, rate = d->unknown_08 >> 8;
            if (frames != 0 && rate != 0) resource += (frame / rate) % frames * 4;
        }
        static const int ORDER[4] = { 0, 1, 2, 3 };
        corners_of(r, o, ORDER, screen);
        const FshEntry *t = scenery_texture(r->art, resource);
        if (t != NULL && on_screen(screen)) ctx->sink.draw_full(ctx->sink.user, t, screen);
        return;
    }
    if (d->type != TRACK_OBJECT_TWO_SIDED || !in_front(r, o, 4, 2)) return;
    static const int FRONT[2][4] = { { 0, 1, 2, 3 }, { 1, 2, 3, 0 } };
    static const int SIDE[2][4] = { { 1, 4, 5, 2 }, { 4, 5, 2, 1 } };
    const FshEntry *front = scenery_texture(r->art, d->resource_id), *side = scenery_texture(r->art, d->resource2_id);
    corners_of(r, o, FRONT[d->flags & 1], screen);
    if (front != NULL && on_screen(screen)) ctx->sink.draw_full_alt(ctx->sink.user, front, screen);
    corners_of(r, o, SIDE[(d->flags >> 1) & 1], screen);
    if (side != NULL && on_screen(screen)) ctx->sink.draw_full_alt(ctx->sink.user, side, screen);
}

/* ---- fences (0044a2b4) ---- */

/* 0044a1c4: a world point to the screen, pushed onto the near plane. */
static void fence_point(const TrackCamera *cam, const DrawContext *ctx, const int32_t world[3], int32_t camera[3],
                        int32_t screen[2])
{
    const int shift = ctx->world_shift;
    const int32_t t[3] = { (world[0] - cam->position[0]) >> shift, (world[1] - cam->position[1]) >> shift,
                           (world[2] - cam->position[2]) >> shift };
    mat3_apply(t, cam->matrix, camera);
    int32_t p[3] = { camera[0], camera[1], camera[2] };
    push_to(p, NEAR_PLANE >> shift, ctx->near_dir);
    view_project(ctx->view, p, screen);
}

/* A fence along one road edge of the row just finished: the edge's two
 * vertices (from the LOD table, per node phase), raised by 2 m, or 0.9 m
 * when the segment's fence byte has bit 0x20. Textured like the row's last
 * polygon, from texture slot 0x20 + (fence & 0x1f). */
static void draw_fence(const TrackRender *r, const TrackCamera *cam, const DrawContext *ctx, const uint8_t pair[2],
                       int base, const TrackUv *uv, int variant, int texture, int tall)
{
    const int a = base + pair[0], b = base + pair[1];
    const int32_t near = NEAR_PLANE >> ctx->world_shift;
    if (a < 0 || b < 0 || a >= r->vertex_count || b >= r->vertex_count || r->camera[b][2] < near) return;
    const int32_t height = tall ? 0x20000 : 0xE666;
    const int32_t top_b[3] = { r->world[b][0], r->world[b][1] + height, r->world[b][2] };
    const int32_t top_a[3] = { r->world[a][0], r->world[a][1] + height, r->world[a][2] };
    int32_t cam_tb[3], cam_ta[3], screen[8];
    fence_point(cam, ctx, top_b, cam_tb, &screen[0]);
    fence_point(cam, ctx, top_a, cam_ta, &screen[2]);
    screen[4] = r->screen[a][0], screen[5] = r->screen[a][1];
    screen[6] = r->screen[b][0], screen[7] = r->screen[b][1];
    if (outcode_shared(&ctx->clip, screen, 4) != 0) return;
    const FshEntry *tex = texture_of(r->art, texture, uv->variant + variant);
    if (tex == NULL) return;
    if (uv->variant == 0) {
        const int32_t *corners[4] = { cam_tb, cam_ta, r->camera[a], r->camera[b] };
        track_draw_subdivided(ctx, tex, uv->u_level, 0, uv->u_cell, 0, corners, screen, near);
    } else {
        ctx->sink.draw_full_alt(ctx->sink.user, tex, screen);
    }
}

/* Both edges, as 0044b510 / 0044ad5c do after a row band. */
static void draw_fences(const TrackRender *r, const TrackCamera *cam, const DrawContext *ctx,
                        const TrackSegmentRecord *rec, int32_t node, int base, const TrackUv *uv, int variant)
{
    const uint8_t fence = rec->fence;
    const int texture = (fence & 0x1F) + 0x20, tall = (fence & 0x20) == 0, phase = (node & 3) * 2;
    const TrackLod *lod = &TRACK_LODS[rec->lod];
    if (fence & TRI_FENCE_LEFT) draw_fence(r, cam, ctx, &lod->fence[8 + phase], base, uv, variant, texture, tall);
    if (fence & TRI_FENCE_RIGHT) draw_fence(r, cam, ctx, &lod->fence[phase], base, uv, variant, texture, tall);
}

static void draw_cars(const DrawContext *ctx, int32_t node)
{
    if (ctx->sink.draw_cars != NULL) ctx->sink.draw_cars(ctx->sink.user, node);
}

/* After a band of rows: each row's objects and the cars one row further
 * (0044b04a, 0044b88f), then the next row. */
static void pass_rows(const TrackRender *r, const DrawContext *ctx, RowWalker *w, int rows)
{
    const int32_t mask = (int32_t)(r->tri->ring_nodes - 1);
    for (int k = 0; k < rows; k++) {
        while (w->cursor >= 0 && r->objects[w->cursor].node == w->node) {
            draw_object(r, ctx, &r->objects[w->cursor], w->frame);
            w->cursor--;
        }
        /* TODO(00459eb8): the skid marks of this row, in the near pass. */
        draw_cars(ctx, (w->node + 1) & mask);
        w->node = (w->node - 1) & mask;
    }
}

/* Segment s and the one after it hold all the polygon's corners. */
static int drawable(const TrackRender *r, int32_t s)
{
    const TrackSegmentRecord *a = crecord(r, s), *b = crecord(r, s + 1);
    return a != NULL && b != NULL && a->valid && b->valid;
}

/* 0044ad5c: segments from the draw distance down to the near ones, whole
 * textures, no subdivision. */
static void draw_far(const TrackRender *r, const TrackCamera *cam, const DrawContext *ctx, int mirrored, RowWalker *w)
{
    const int32_t node = cam->node;
    int row = TRACK_DRAW_SEGMENTS * 4 - (node & 3) + 3;
    int rows_left = TRACK_DRAW_SEGMENTS * 4 - 4;
    int base = r->vertex_count - ROW_POINTS;
    int32_t segment = node / 4 + TRACK_DRAW_SEGMENTS - 1;
    int variant = variant_far(row);
    while (TRACK_NEAR_SEGMENTS * 4 <= rows_left) {
        const TrackSegmentRecord *rec = crecord(r, segment);
        if (rec == NULL) break;
        const TrackLod *lod = &TRACK_LODS[rec->lod];
        base -= lod->vertex_count;
        int rows_in_segment = 0;
        for (int i = lod->polygon_count - 1; i >= 0; i--) {
            const TrackPolygon *p = &TRACK_POLYGONS[lod->first_polygon + i];
            int32_t screen[8];
            const int32_t *corners[4];
            gather(r, base, p, screen, corners);
            if (drawable(r, segment) && visible_far(&ctx->clip, screen, p->strip, mirrored) && corners[3][2] > 0) {
                const FshEntry *tex = texture_of(r->art, rec->texture[p->strip], TRACK_UVS[p->uv].variant + variant);
                if (tex != NULL) ctx->sink.draw_full(ctx->sink.user, tex, screen);
            }
            if (p->rows_done != 0) {
                if (drawable(r, segment)) draw_fences(r, cam, ctx, rec, w->node, base, &TRACK_UVS[p->uv], variant);
                pass_rows(r, ctx, w, p->rows_done);
                row -= p->rows_done;
                rows_left -= p->rows_done;
                rows_in_segment += p->rows_done;
                variant = variant_far(row);
            }
        }
        segment -= rows_in_segment > 0 ? (rows_in_segment + 3) / 4 : 1;
    }
}

/* 0044b510: the nearest segments, subdivided. */
static void draw_near(const TrackRender *r, const TrackCamera *cam, const DrawContext *ctx, int mirrored,
                      RowWalker *w)
{
    const int32_t node = cam->node;
    const int segments = TRACK_DRAW_SEGMENTS < TRACK_NEAR_SEGMENTS ? TRACK_DRAW_SEGMENTS : TRACK_NEAR_SEGMENTS;
    int base = r->vertex_count - ROW_POINTS - (TRACK_DRAW_SEGMENTS - segments) * ROW_POINTS;
    int row = segments * 4 - (node & 3) + 3;
    int variant = variant_near(row);
    const int32_t near = NEAR_PLANE >> ctx->world_shift;
    for (int k = segments - 1; k >= 0; k--) {
        const int32_t segment = node / 4 + k;
        const TrackSegmentRecord *rec = crecord(r, segment);
        if (rec == NULL) continue;
        const TrackLod *lod = &TRACK_LODS[rec->lod];
        base -= rec->count;
        for (int i = lod->polygon_count - 1; i >= 0; i--) {
            const TrackPolygon *p = &TRACK_POLYGONS[lod->first_polygon + i];
            int32_t screen[8];
            const int32_t *corners[4];
            gather(r, base, p, screen, corners);
            if (drawable(r, segment) && visible_near(&ctx->clip, screen, p->strip, mirrored)) {
                const TrackUv *uv = &TRACK_UVS[p->uv];
                const FshEntry *tex = texture_of(r->art, rec->texture[p->strip], uv->variant + variant);
                if (tex != NULL) track_draw_subdivided(ctx, tex, uv->u_level, 0, uv->u_cell, 0, corners, screen, near);
            }
            if (p->rows_done != 0) {
                if (drawable(r, segment)) draw_fences(r, cam, ctx, rec, w->node, base, &TRACK_UVS[p->uv], variant);
                pass_rows(r, ctx, w, p->rows_done);
                row -= p->rows_done;
                variant = variant_near(row);
            }
        }
    }
}

void track_render_draw(const TrackRender *r, const TrackCamera *cam, const DrawContext *ctx, int mirrored,
                       int32_t frame)
{
    if (r->vertex_count == 0) return;
    const int32_t mask = (int32_t)(r->tri->ring_nodes - 1);
    RowWalker w = { ((cam->node & ~3) + TRACK_DRAW_SEGMENTS * 4 - 1) & mask, r->object_count - 1, frame };
    draw_far(r, cam, ctx, mirrored, &w);
    w.node = ((cam->node & ~3) + TRACK_NEAR_SEGMENTS * 4 - 1) & mask;
    draw_near(r, cam, ctx, mirrored, &w);
    draw_cars(ctx, (w.node + 1) & mask);  /* 0044b945 */
}
