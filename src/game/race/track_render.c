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

static void rotate_x(int32_t m[9], int32_t a)  /* 00449520 */
{
    const int32_t s = fx_sin16(a >> 8), c = fx_cos16(a >> 8);
    const int32_t r[9] = { 0x10000, 0, 0, 0, c, -s, 0, s, c };
    memcpy(m, r, sizeof r);
}

static void rotate_y(int32_t m[9], int32_t a)  /* 0044957c */
{
    const int32_t s = fx_sin16(a >> 8), c = fx_cos16(a >> 8);
    const int32_t r[9] = { c, 0, s, 0, 0x10000, 0, -s, 0, c };
    memcpy(m, r, sizeof r);
}

static void rotate_z(int32_t m[9], int32_t a)  /* 004495d8 */
{
    const int32_t s = fx_sin16(a >> 8), c = fx_cos16(a >> 8);
    const int32_t r[9] = { c, -s, 0, s, c, 0, 0, 0, 0x10000 };
    memcpy(m, r, sizeof r);
}

void track_camera_matrix(int32_t pitch, int32_t yaw, int32_t roll, int mirrored, int32_t out[9])
{
    int32_t x[9], y[9], z[9], t[9], u[9];
    rotate_x(x, pitch);
    rotate_z(z, roll);
    rotate_y(y, yaw);
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

/* 0043eca4, terrain part: the LOD's selected points of the first `rows`
 * rows, appended to the vertex list. */
static void add_segment(TrackRender *r, int32_t segment, int rows)
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
    }
    rec->count = r->vertex_count - rec->first;
}

void track_render_build(TrackRender *r, const Tri *tri, const TrackArt *art, int32_t camera_node)
{
    memset(r->segments, 0, sizeof r->segments);
    r->tri = tri;
    r->art = art;
    r->vertex_count = 0;
    r->first_segment = camera_node / 4;  /* rounds toward zero, as the original */
    int32_t s = r->first_segment;
    for (int i = 0; i < TRACK_DRAW_SEGMENTS - 1; i++, s++) add_segment(r, s, 4);
    add_segment(r, s, 4);
    add_segment(r, s + 1, 1);
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

/* Segment s and the one after it hold all the polygon's corners. */
static int drawable(const TrackRender *r, int32_t s)
{
    const TrackSegmentRecord *a = crecord(r, s), *b = crecord(r, s + 1);
    return a != NULL && b != NULL && a->valid && b->valid;
}

/* 0044ad5c: segments from the draw distance down to the near ones, whole
 * textures, no subdivision. */
static void draw_far(const TrackRender *r, const TrackCamera *cam, const DrawContext *ctx, int mirrored)
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
static void draw_near(const TrackRender *r, const TrackCamera *cam, const DrawContext *ctx, int mirrored)
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
                row -= p->rows_done;
                variant = variant_near(row);
            }
        }
    }
}

void track_render_draw(const TrackRender *r, const TrackCamera *cam, const DrawContext *ctx, int mirrored)
{
    if (r->vertex_count == 0) return;
    draw_far(r, cam, ctx, mirrored);
    draw_near(r, cam, ctx, mirrored);
}
