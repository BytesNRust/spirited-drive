/* The rear-view mirror's scene (0044baa4). */
#include "game/race/mirror_scene.h"

#include "game/gfx/x86_int.h"
#include "game/race/track_tables.h"
#include "game/sys/fixed_math.h"

#include <string.h>

void mirror_bounds(const ViewLayout *layout, MirrorBounds *b)
{
    b->left = layout->left;
    b->top = layout->top;
    b->right = x_add(b->left, layout->unknown_20);
    b->bottom = x_add(b->top, layout->unknown_24);
}

uint32_t mirror_segment_offset(int32_t segment, int32_t mask)
{
    return ((uint32_t)(segment & mask) * MIRROR_SEGMENT_RECORD) % MIRROR_SEGMENT_CACHE;
}

enum { PHYSICS_SKIPPED = 6, MIRROR_PASS = 1 };

enum { LATERAL_BIAS = 0x29999, LATERAL_NEXT = 0x50000 };

void mirror_car_rows(Car *const *cars, int32_t count, int32_t segments, int32_t camera_node, uint8_t rows[MIRROR_CAR_ROWS])
{
    memset(rows, 0, MIRROR_CAR_ROWS);
    const int32_t ring = x_shl(segments, 2);
    for (int32_t i = 0; i < count; i++) {
        Car *car = cars[i];
        if (car->physics == PHYSICS_SKIPPED) continue;
        int32_t node = car->draw_node2;
        if (x_add(car->lateral, LATERAL_BIAS) > LATERAL_NEXT) node = x_add(node, 1);
        if (node > ring) node = x_sub(node, ring);
        car->draw_node2 = node;
        const int32_t behind = node <= camera_node ? x_sub(camera_node, node) : x_add(x_sub(ring, node), camera_node);
        if (behind >= MIRROR_CAR_ROWS || behind < 0) continue;  /* port guard: < 0 writes before 0051c7a1 */
        rows[behind] = 1;
    }
}

void mirror_cars(const MirrorCarsOps *ops, const MirrorCarsInputs *in, int32_t node)
{
    const int32_t behind = node <= in->camera_node ? x_sub(in->camera_node, node)
                                                   : x_add(in->camera_node, x_sub(x_shl(in->segments, 2), node));
    if (behind >= MIRROR_CAR_ROWS || behind < 0 || in->rows[behind] == 0) return;
    for (int32_t i = 0; i <= x_sub(in->count, 1); i++) {
        Car *car = in->list[i];
        if (car->physics == PHYSICS_SKIPPED || node != car->draw_node2) continue;
        if (car == in->followed && in->chase_kind == 0) continue;
        ops->prepare(ops->ctx, car, MIRROR_PASS);
        ops->draw(ops->ctx, car, MIRROR_PASS);
    }
}

/* 004c2e94: the variant added by distance, nearest segment last. */
static const int32_t MIRROR_VARIANT_ROWS[MIRROR_SEGMENTS] = { 1, 1, 1, 1, 1 };

enum { CARS_BEHIND = 5, CARS_ROWS = 5, CARS_AFTER_A = 2, CARS_AFTER_B = 1, SEGMENT_STRIDE = 6, SEGMENT_LOD = 1,
       RECORD_TEXTURES = 2, FLAT_VARIANTS = 8, SEGMENTS_BEHIND = 4 };

static int32_t at_least_zero(int32_t v)
{
    return v < 0 ? 0 : v;
}

static void scene_polygon(const MirrorSceneOps *ops, const MirrorSceneInputs *in, const MirrorBounds *box,
                          const TrackPolygon *p, int32_t base, const uint8_t *record, int32_t row)
{
    int32_t screen[8];
    const int32_t *cam[4];
    for (int i = 0; i < 4; i++) {
        const int32_t v = base + p->corner[i];
        screen[2 * i] = in->vertices[MIRROR_SCREEN_WORDS_AT + 2 * v];
        screen[2 * i + 1] = in->vertices[MIRROR_SCREEN_WORDS_AT + 2 * v + 1];
        cam[i] = in->vertices + 3 * v;
    }
    if (!mirror_quad_visible(box, screen)) return;
    const TrackUv *uv = &TRACK_UVS[p->uv];
    const int32_t flat = record[RECORD_TEXTURES + p->strip] * FLAT_VARIANTS + uv->variant + MIRROR_VARIANT_ROWS[row];
    const FshEntry *t = ops->texture(ops->ctx, flat);
    if (t != NULL) ops->subdivide(ops->ctx, t, uv->u_level, uv->u_cell, cam, screen, in->near);
}

void mirror_scene(const MirrorSceneOps *ops, const MirrorSceneInputs *in, int32_t count)
{
    if (in->skip != 0) return;
    MirrorBounds box;
    mirror_bounds(in->layout, &box);
    int32_t segment = (x_add(in->camera_node, 3) / 4 - SEGMENTS_BEHIND) & in->segment_mask;
    if (in->circuit == 0 && segment < 0) segment = 0;
    int32_t node = x_shl(segment, 2);
    if (count > MIRROR_SEGMENTS) count = MIRROR_SEGMENTS;  /* port guard: 004c2e94 and 00509330 hold 5 */
    for (int32_t k = 0, row = count - 1; row >= 0; k++, row--) {
        const TrackLod *lod = &TRACK_LODS[in->segments[(uint32_t)segment * SEGMENT_STRIDE + SEGMENT_LOD]];
        const uint8_t *record = in->cache + mirror_segment_offset(segment, in->segment_mask);
        for (int32_t p = lod->first_polygon + lod->polygon_count - 1; p >= lod->first_polygon; p--) {
            const TrackPolygon *poly = &TRACK_POLYGONS[p];
            scene_polygon(ops, in, &box, poly, k * MIRROR_ROW_POINTS, record, row);
            if (poly->rows_done == 0) continue;
            for (int i = 0; i < CARS_ROWS; i++) {
                ops->cars(ops->ctx, at_least_zero(x_sub(node, CARS_BEHIND)));
                node = x_add(node, 1) & in->node_mask;
            }
        }
        segment = (segment + 1) & in->segment_mask;
    }
    ops->cars(ops->ctx, at_least_zero(x_sub(node, CARS_AFTER_A)));
    ops->cars(ops->ctx, at_least_zero(x_sub(node, CARS_AFTER_B)));
}

enum { MIRROR_CLOSE = 0x23333 };

void mirror_transform(const MirrorTransformInputs *in, int32_t *near, int32_t *vertices)
{
    const int shift = in->world_shift & 31;
    *near = in->near_plane >> shift;
    const int32_t offset[3] = { x_sub(0, in->position[0]) >> shift, x_sub(0, in->position[1]) >> shift,
                                x_sub(0, in->position[2]) >> shift };
    const int32_t count = in->count < MIRROR_POINTS ? in->count : MIRROR_POINTS;  /* port guard: 00509330 holds 55 */
    for (int32_t i = 0; i < count; i++) {
        const int32_t t[3] = { x_add(in->world[i][0] >> shift, offset[0]), x_add(in->world[i][1] >> shift, offset[1]),
                               x_add(in->world[i][2] >> shift, offset[2]) };
        mat3_apply(t, in->matrix, vertices + 3 * i);
    }
    int32_t *screen = vertices + MIRROR_SCREEN_WORDS_AT;
    for (int32_t i = 0; i < count; i++) view_project(in->view, vertices + 3 * i, screen + 2 * i);
    const int32_t close = MIRROR_CLOSE >> shift;
    for (int32_t i = 0; i < count; i++) {
        if (close <= vertices[3 * i + 2]) continue;
        const int32_t pushed[3] = { vertices[3 * i], vertices[3 * i + 1], x_add(close, close) };
        view_project(in->view, pushed, screen + 2 * i);
    }
}

/* 004bc464 (0x18 bytes a road property, its first word): the mirror's LOD. */
static const uint8_t MIRROR_LOD_BY_PROPERTY[TRACK_PROPERTIES] = { 7, 3, 10, 3, 3, 3, 3, 18, 3, 21, 3, 3, 25, 28, 3, 3, 3,
                                                                  3, 3 };

enum { RECORD_ROW = 0xc, ROW_POINT_BYTES = 12 };

static int32_t record_word(const uint8_t *b)
{
    return (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
}

void mirror_row(const MirrorRowOps *ops, const uint8_t *cache, int32_t segment_mask, int32_t segment,
                int32_t (*world)[3], int32_t *count, uint8_t *segments)
{
    const uint8_t *record = cache + mirror_segment_offset(segment, segment_mask);
    const uint8_t property = ops->property(ops->ctx, x_shl(segment, 2));
    if (property >= TRACK_PROPERTIES) return;  /* port guard: the original reads past 004bc464 */
    const uint8_t lod = MIRROR_LOD_BY_PROPERTY[property];
    const uint8_t *select = TRACK_LODS[lod].select;
    for (int p = 0; p < MIRROR_ROW_POINTS; p++) {
        if (*select != p) continue;
        select++;
        if (*count >= MIRROR_POINTS) continue;  /* port guard: 0050909c holds 55 */
        const uint8_t *point = record + RECORD_ROW + ROW_POINT_BYTES * p;
        for (int c = 0; c < 3; c++) world[*count][c] = record_word(point + 4 * c);
        *count = x_add(*count, 1);
    }
    segments[(uint32_t)(segment & segment_mask) * SEGMENT_STRIDE + SEGMENT_LOD] = lod;
}

enum { MIRROR_ON = 1, MIRROR_SKIP = 2, MIRROR_DRAWN_BELOW = 2 };

void mirror_frame(const MirrorFrameOps *ops, const MirrorFrameInputs *in, int32_t *state)
{
    if (in->wanted != 0) *state |= MIRROR_ON;
    else *state = 0;
    if ((*state & MIRROR_ON) == 0) return;
    if (*state == MIRROR_ON && in->unknown_4b91c8 < MIRROR_DRAWN_BELOW) {
        ops->view(ops->ctx, 1);
        ops->transform(ops->ctx);
        ops->select(ops->ctx);
        ops->sky(ops->ctx);
        ops->scene(ops->ctx, in->count);
        ops->flip(ops->ctx);
        ops->view(ops->ctx, 0);
    }
    if (in->alternate != 0) *state ^= MIRROR_SKIP;
}

void *mirror_create(void *(*make)(void *ctx, int32_t width, int32_t height, int32_t flags), void *ctx,
                    const int32_t box[4], int32_t *width, int32_t *height)
{
    *width = x_add(x_sub(box[2], box[0]), 1);
    *height = x_add(x_sub(box[3], box[1]), 1);
    return make(ctx, *width, *height, 0);
}

void mirror_free(void (*release)(void *ctx, void *picture), void *ctx, void **picture)
{
    if (*picture == NULL) return;
    release(ctx, *picture);
    *picture = NULL;
}

void mirror_place(void (*place)(void *ctx, int32_t x, int32_t y), void *ctx, int32_t state, int32_t x, int32_t y)
{
    if (state != 0) place(ctx, x, y);
}

static void midpoint(const int32_t *a, const int32_t *b, int32_t out[3])
{
    for (int i = 0; i < 3; i++) out[i] = x_add(a[i], b[i]) / 2;
}

/* 00425e68: a quarter's corners and screen points copied together, then 00425ed4 on them */
static void quarter(const DrawContext *ctx, const FshEntry *t, int u_level, int depth, int u_cell, int v_cell,
                    const int32_t *c0, const int32_t *c1, const int32_t *c2, const int32_t *c3, const int32_t *s0,
                    const int32_t *s1, const int32_t *s2, const int32_t *s3, int32_t near)
{
    const int32_t *const cam[4] = { c0, c1, c2, c3 };
    const int32_t screen[8] = { s0[0], s0[1], s1[0], s1[1], s2[0], s2[1], s3[0], s3[1] };
    mirror_draw_subdivided(ctx, t, u_level, depth, u_cell, v_cell, cam, screen, near);
}

void mirror_draw_subdivided(const DrawContext *ctx, const FshEntry *texture, int u_level, int depth, int u_cell,
                            int v_cell, const int32_t *const cam[4], const int32_t screen[8], int32_t near)
{
    if (ctx->sink.draw_camera != NULL) {  /* port: the whole cell, clipped and perspective-correct (as 00425718's) */
        const int32_t w = (int16_t)texture->width >> (u_level & 31), h = (int16_t)texture->height >> (depth & 31);
        const int32_t u0 = ((1 << u_level) - 1 - u_cell) * w << 16, v0 = h * ((1 << depth) - 1 - v_cell) << 16;
        const int32_t u1 = u0 + ((w - 1) << 16), v1 = v0 + ((h - 1) << 16);
        const int32_t uv[8] = { u0, v0, u1, v0, u1, v1, u0, v1 };
        ctx->sink.draw_camera(ctx->sink.user, ctx->view, texture, uv, cam);
        return;
    }
    if (near > cam[2][2] && near > cam[1][2]) return;
    const int32_t limit = TRACK_SUBDIVIDE_Z[depth] >> ctx->world_shift;
    if (limit <= cam[3][2] || depth >= TRACK_SUBDIVIDE_MAX) {
        track_draw_cell(ctx, texture, u_level, depth, u_cell, v_cell, screen);
        return;
    }
    int32_t m01[3], m23[3], mc[3], m12[3], m03[3];
    int32_t s01[2], s23[2], sc[2], s12[2], s03[2];
    midpoint(cam[0], cam[1], m01);
    track_project_near(ctx, m01, near, s01);
    midpoint(cam[2], cam[3], m23);
    track_project_near(ctx, m23, near, s23);
    midpoint(m23, m01, mc);
    track_project_near(ctx, mc, near, sc);
    midpoint(cam[1], cam[2], m12);
    track_project_near(ctx, m12, near, s12);
    const int u = u_level + 1, d = depth + 1;
    quarter(ctx, texture, u, d, 2 * u_cell + 1, 2 * v_cell + 1, m01, cam[1], m12, mc, s01, &screen[2], s12, sc, near);
    quarter(ctx, texture, u, d, 2 * u_cell + 1, 2 * v_cell, mc, m12, cam[2], m23, sc, s12, &screen[4], s23, near);
    if (m23[2] < near && near > m01[2]) return;
    midpoint(cam[0], cam[3], m03);
    track_project_near(ctx, m03, near, s03);
    quarter(ctx, texture, u, d, 2 * u_cell, 2 * v_cell + 1, cam[0], m01, mc, m03, &screen[0], s01, sc, s03, near);
    quarter(ctx, texture, u, d, 2 * u_cell, 2 * v_cell, m03, mc, m23, cam[3], s03, sc, s23, &screen[6], near);
}

int mirror_quad_visible(const MirrorBounds *b, const int32_t screen[8])
{
    int left = 1, right = 1;
    for (int i = 0; i < 4; i++) {
        if (screen[2 * i] >= b->left) left = 0;
        if (screen[2 * i] <= b->right) right = 0;
    }
    if (left || right) return 0;
    return b->bottom >= screen[3] || b->bottom >= screen[5];
}
