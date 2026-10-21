#include "game/race/track_far.h"

#include "game/gfx/x86_int.h"
#include "game/race/mirror_scene.h"
#include "game/race/track_render.h"
#include "game/race/track_tables.h"

enum {
    ROW_POINTS = 11,
    FENCE_RIGHT = 0x80,   /* the LOD's +0x10 pairs */
    FENCE_LEFT = 0x40,    /* its +8 pairs */
    FENCE_LOW = 0x20,
    FENCE_TEXTURE = 0x20,
    VARIANTS = 8          /* 0050eec4: 8 dwords a texture */
};

/* 004c2f20 */
static int far_level(int32_t band)
{
    const int32_t row = band / 4;
    return row >= 0 && row < TRACK_VARIANT_ROWS ? TRACK_VARIANT_FAR[row] : 0;  /* port guard */
}

static const TrackLod *lod_of(const TrackFarInputs *in, int32_t segment)
{
    const unsigned lod = in->segments[(uint32_t)segment & in->segment_mask].lod;
    return &TRACK_LODS[lod < TRACK_LOD_COUNT ? lod : 0];  /* port guard */
}

static void quad_of(const TrackFarInputs *in, int32_t first, const TrackPolygon *p, int32_t quad[8])
{
    for (int k = 0; k < 4; k++) {
        quad[2 * k] = in->fence.screen[first + p->corner[k]][0];
        quad[2 * k + 1] = in->fence.screen[first + p->corner[k]][1];
    }
}

/* The fences of a row band's last polygon, on the row's node `at`. */
static void draw_fences(const TrackFarInputs *in, const TrackLod *lod, const uint8_t *record, const TrackUv *uv,
                        int32_t first, int32_t at, int level)
{
    const uint8_t fence = record[1];
    const int32_t texture = (fence & 0x1f) + FENCE_TEXTURE;
    const int tall = (fence & FENCE_LOW) == 0;
    const int pair = (at & 3) * 2;
    if (fence & FENCE_RIGHT) track_fence_draw(in->fence_ops, &in->fence, &lod->fence[8 + pair], first, uv, level, texture, tall);
    if (fence & FENCE_LEFT) track_fence_draw(in->fence_ops, &in->fence, &lod->fence[pair], first, uv, level, texture, tall);
}

/* 0044ad5c */
void track_far_draw(const TrackFarOps *ops, const TrackFarInputs *in, int32_t segments, int32_t node, int32_t near)
{
    TrackObjectWalk *walk = in->objects->walk;
    const uint32_t mask = walk->node_mask;
    int32_t first = x_sub(in->vertex_count, ROW_POINTS);
    int32_t band = x_add(x_sub(x_sub(x_shl(segments, 2), 1), node & 3), 4);
    int level = far_level(band);
    int32_t segment = x_sub(x_add(node / 4, segments), 1);
    int32_t at = (int32_t)((uint32_t)x_sub(x_add(node & ~3, x_shl(segments, 2)), 1) & mask);
    int32_t rows = x_sub(x_shl(segments, 2), 4);
    const int32_t stop = x_shl(near, 2);
    *in->quarter = (uint8_t)(1u << (node & 3));
    if (in->camera_flag != 0) *in->quarter = (uint8_t)(*in->quarter << 4);
    while (rows >= stop) {
        const TrackLod *lod = lod_of(in, segment);
        first = x_sub(first, lod->vertex_count);
        const uint8_t *record = in->cache + mirror_segment_offset(segment, (int32_t)in->segment_mask);
        for (int32_t i = lod->polygon_count - 1; i >= 0; i--) {
            const int32_t index = lod->first_polygon + i;
            if (index < 0 || index >= TRACK_POLYGON_COUNT) continue;  /* port guard */
            const TrackPolygon *p = &TRACK_POLYGONS[index];
            const TrackUv *uv = &TRACK_UVS[p->uv];
            int32_t quad[8];
            quad_of(in, first, p, quad);
            if (track_visible_far(&in->fence.clip, quad, p->strip, in->mirrored) &&
                in->fence.camera[first + p->corner[3]][2] > 0) {
                const FshEntry *t = ops->texture(ops->ctx, record[2 + p->strip] * VARIANTS + uv->variant + level);
                if (t != NULL && ops->full_camera != NULL) {
                    const int32_t *const cam[4] = { in->fence.camera[first + p->corner[0]], in->fence.camera[first + p->corner[1]],
                                                    in->fence.camera[first + p->corner[2]], in->fence.camera[first + p->corner[3]] };
                    ops->full_camera(ops->ctx, t, cam);
                } else if (t != NULL) {
                    ops->full(ops->ctx, t, quad);
                }
            }
            if (p->rows_done == 0) continue;
            draw_fences(in, lod, record, uv, first, at, level);
            for (int k = p->rows_done; k > 0; k--) {
                while (at == walk->node) track_object_draw(in->objects, band, 1);
                ops->cars(ops->ctx, (int32_t)((uint32_t)(at + 1) & mask));
                at = (int32_t)((uint32_t)(at - 1) & mask);
            }
            band = x_sub(band, p->rows_done);
            rows = x_sub(rows, p->rows_done);
            level = far_level(band);
        }
        segment = at >> 2;
    }
}
