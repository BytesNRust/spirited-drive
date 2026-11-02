#include "game/race/track_near.h"

#include "game/gfx/x86_int.h"
#include "game/race/track_render.h"
#include "game/race/track_tables.h"
#include "game/race/track_wall.h"

enum {
    ROW_POINTS = 11,
    FENCE_RIGHT = 0x80,   /* the LOD's +0x10 pairs */
    FENCE_LEFT = 0x40,    /* its +8 pairs */
    FENCE_LOW = 0x20,
    FENCE_TEXTURE = 0x20,
    VARIANTS = 8,         /* 0050eec4: 8 dwords a texture */
    MARKED = 8            /* record +8: drawn with its row, not after it */
};

/* 004c2ea8 */
static int near_level(int32_t band)
{
    return band >= 0 && band < TRACK_VARIANT_ROWS ? TRACK_VARIANT_NEAR[band] : 0;  /* port guard */
}

static void fences(const TrackFarInputs *f, const TrackLod *lod, const uint8_t *record, const TrackUv *uv, int32_t first,
                   int32_t at, int level)
{
    const uint8_t fence = record[1];
    const int32_t texture = (fence & 0x1f) + FENCE_TEXTURE;
    const int tall = (fence & FENCE_LOW) == 0;
    const int pair = (at & 3) * 2;
    if (fence & FENCE_RIGHT) track_fence_draw(f->fence_ops, &f->fence, &lod->fence[8 + pair], first, uv, level, texture, tall);
    if (fence & FENCE_LEFT) track_fence_draw(f->fence_ops, &f->fence, &lod->fence[pair], first, uv, level, texture, tall);
}

static int under_walker_unmarked(const TrackObjectWalk *w, int32_t at)
{
    return w->node == at && w->records[(uint32_t)w->cursor * TRACK_WALK_RECORD_SIZE + MARKED] == 0;
}

/* 0044b510 */
void track_near_draw(const TrackFarOps *far_ops, const TrackNearOps *ops, const TrackNearInputs *in, int32_t segments,
                     int32_t node)
{
    const TrackFarInputs *f = in->far;
    TrackObjectWalk *walk = f->objects->walk;
    const uint32_t mask = walk->node_mask;
    const int32_t near = segments > TRACK_NEAR_SEGMENTS ? TRACK_NEAR_SEGMENTS : segments;
    mirror_bounds(in->layout, in->bounds);
    if (in->alternate == 0) track_far_draw(far_ops, f, segments, node, near);
    int32_t first = x_sub(x_sub(f->vertex_count, ROW_POINTS), x_mul(x_sub(segments, near), ROW_POINTS));
    int32_t band = x_add(x_sub(x_sub(x_shl(near, 2), 1), node & 3), 4);
    int level = near_level(band);
    int32_t segment = (int32_t)((uint32_t)x_sub(x_add(node / 4, near), 1) & f->segment_mask);
    int32_t at = (int32_t)((uint32_t)x_sub(x_add(node & ~3, x_shl(near, 2)), 1) & mask);
    for (int32_t k = near - 1; k >= 0; k--) {
        const TrackSegmentEntry *e = &f->segments[(uint32_t)segment & f->segment_mask];
        const TrackLod *lod = track_wall_lod(e->lod < TRACK_LOD_COUNT ? e->lod : 0);  /* port guard; a reversed circuit's walls where they were */
        first = x_sub(first, e->count);
        const uint8_t *record = f->cache + mirror_segment_offset(segment, (int32_t)f->segment_mask);
        for (int32_t i = lod->polygon_count - 1; i >= 0; i--) {
            const int32_t index = lod->first_polygon + i;
            if (index < 0 || index >= TRACK_POLYGON_COUNT) continue;  /* port guard */
            const TrackPolygon *p = &TRACK_POLYGONS[index];
            const TrackUv *uv = &TRACK_UVS[p->uv];
            int32_t quad[8];
            const int32_t *corners[4];
            for (int c = 0; c < 4; c++) {
                quad[2 * c] = f->fence.screen[first + p->corner[c]][0];
                quad[2 * c + 1] = f->fence.screen[first + p->corner[c]][1];
                corners[c] = f->fence.camera[first + p->corner[c]];
            }
            if (track_visible_near(in->bounds, quad, p->strip, f->mirrored)) {
                const FshEntry *t = f->fence_ops->texture(f->fence_ops->ctx, record[2 + p->strip] * VARIANTS + uv->variant + level);
                track_light_set(&f->fence, first, p->corner);
                if (t != NULL)
                    f->fence_ops->subdivide(f->fence_ops->ctx, t, uv->u_level, uv->u_cell, corners, quad, f->fence.near);
                track_light_clear(&f->fence);
            }
            if (p->rows_done == 0) continue;
            fences(f, lod, record, uv, first, at, level);
            for (int r = p->rows_done; r > 0; r--) {
                while (at == walk->node) track_object_draw(f->objects, band, 1);
                ops->skids(ops->ctx, node, at, &quad[0], &quad[6]);
                far_ops->cars(far_ops->ctx, (int32_t)((uint32_t)(at + 1) & mask));
                at = (int32_t)((uint32_t)(at - 1) & mask);
                while (under_walker_unmarked(walk, at)) track_object_draw(f->objects, band, 1);
            }
            band = x_sub(band, p->rows_done);
            level = near_level(band);
        }
        segment = (int32_t)((uint32_t)(segment - 1) & f->segment_mask);
    }
    far_ops->cars(far_ops->ctx, (int32_t)((uint32_t)(at + 1) & mask));
    if (in->alternate != 0) ops->far_alternate(ops->ctx, segments, node, near);
}
