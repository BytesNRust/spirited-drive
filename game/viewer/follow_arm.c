/* The road behind the camera's node (follow_arm.h). */
#include "game/viewer/follow_arm.h"

#include "game/gfx/x86_int.h"
#include "game/race/mirror_scene.h"
#include "game/race/track_tables.h"
#include "game/sys/fixed_math.h"
#include "game/race/view.h"

#include <string.h>

enum {
    ROW = MIRROR_ROW_POINTS,
    ROWS = FOLLOW_ARM_SEGMENTS + 2,   /* the behind arm also draws the camera's own segment */
    RECORD_ROW = 0xc, POINT_BYTES = 12, RECORD_TEXTURES = 2, FLAT_VARIANTS = 8, NEAR_VARIANT = 1, DETAIL_VARIANT = 0, DETAIL_SEGMENTS = 3,
    PROPERTY_AT = 7, CACHE_SEGMENTS = 62, MIRROR_CLOSE = 0x23333
};

/* 004bc464: the mirror's LOD by a road property (mirror_scene.c's) */
static const uint8_t LOD_BY_PROPERTY[TRACK_PROPERTIES] = { 7, 3, 10, 3, 3, 3, 3, 18, 3, 21, 3, 3, 25, 28, 3, 3, 3, 3, 3 };

static int32_t word(const uint8_t *b)
{
    return (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
}

typedef struct {
    int32_t world[ROWS * ROW][3], camera[ROWS * ROW][3], screen[ROWS * ROW][2];
    uint8_t lod[ROWS];
} Rows;

static Rows rows;


/* port, the classic sink: a corner nearer than `plane` is slid along its quad's edge to the plane, towards a corner in front
 * (an adjacent one, else the opposite; two adjacent ones are averaged), so a quad across the camera still projects as a
 * quad (its texture is squeezed into what is left of it). Returns 0 when no corner is in front. */
static int clip_quad(const int32_t *const in[4], int32_t plane, int32_t out[4][3])
{
    int front = 0;
    for (int c = 0; c < 4; c++) front += in[c][2] >= plane;
    if (front == 0) return 0;
    for (int c = 0; c < 4; c++) {
        if (in[c][2] >= plane) {
            memcpy(out[c], in[c], sizeof out[c]);
            continue;
        }
        const int towards[2] = { (c + 3) & 3, (c + 1) & 3 };
        int32_t sum[3] = { 0, 0, 0 };
        int used = 0;
        for (int n = 0; n < 2; n++) {
            const int32_t *to = in[towards[n]];
            if (to[2] < plane) continue;
            const int64_t span = (int64_t)to[2] - in[c][2], t = (int64_t)plane - in[c][2];
            for (int a = 0; a < 3; a++) sum[a] += (int32_t)(in[c][a] + (to[a] - in[c][a]) * t / span);
            used++;
        }
        if (used == 0) {
            const int32_t *to = in[(c + 2) & 3];
            const int64_t span = (int64_t)to[2] - in[c][2], t = (int64_t)plane - in[c][2];
            for (int a = 0; a < 3; a++) sum[a] = (int32_t)(in[c][a] + (to[a] - in[c][a]) * t / span);
            used = 1;
        }
        for (int a = 0; a < 3; a++) out[c][a] = sum[a] / used;
    }
    return 1;
}

void follow_arm_window(TrackRender *r, int32_t segments)
{
    /* the segments loaded when the window starts again: the exe's five behind, or `segments` when they fit the 62 records */
    int32_t behind = segments > 5 ? segments : 5;
    if (r->window.ahead + behind > CACHE_SEGMENTS - 2) behind = CACHE_SEGMENTS - 2 - r->window.ahead;
    if (behind > 5) r->window.behind = r->window.ahead + behind;
}

static const FshEntry *arm_texture(const TrackArt *art, int32_t flat)
{
    if (flat < 0 || flat >= TRACK_TEXTURE_SLOTS * TRACK_TEXTURE_VARIANTS) return NULL;
    const TrackTexture *t = &art->slots[flat / TRACK_TEXTURE_VARIANTS][flat % TRACK_TEXTURE_VARIANTS];
    return t->present ? &t->bitmap : NULL;
}

void follow_arm_draw(const FollowArm *a, int32_t segments, int ahead)
{
    const TrackRender *r = a->render;
    const TrackCamera *cam = a->camera;
    const DrawContext *ctx = a->ctx;
    if (segments > FOLLOW_ARM_SEGMENTS) segments = FOLLOW_ARM_SEGMENTS;
    const int32_t mask = (int32_t)r->build.segment_mask;
    const int32_t camera_segment = cam->node / 4;
    int32_t first = ahead ? camera_segment : camera_segment - segments;
    if (!a->circuit && first < 0) first = 0;
    const int32_t count = ahead ? segments : camera_segment - first + 1;   /* segments drawn: rows 0 .. count; behind, the camera's own too, half of it is in view */
    if (count <= 0 || first + count + 1 >= TRACK_SEGMENT_ENTRIES) return;
    const uint32_t node_mask = r->build.walk.node_mask;
    /* the rows' points, whole first rows of the cache's records */
    for (int32_t i = 0; i <= count; i++) {
        const int32_t segment = (first + i) & mask;
        const uint8_t *record = r->cache + mirror_segment_offset(segment, mask);
        const uint8_t property = r->nodes[(((uint32_t)segment << 2) & node_mask) * TRACK_BUILD_NODE_SIZE + PROPERTY_AT];
        rows.lod[i] = LOD_BY_PROPERTY[property < TRACK_PROPERTIES ? property : 1];
        for (int p = 0; p < ROW; p++)
            for (int c = 0; c < 3; c++) rows.world[i * ROW + p][c] = word(record + RECORD_ROW + POINT_BYTES * p + 4 * c);
    }
    /* camera space and screen as the mirror's transform does it (0044b970), the whole arm's points */
    const int shift = ctx->world_shift & 31;
    const int32_t near = track_render_near(ctx);
    const int32_t offset[3] = { x_sub(0, cam->position[0]) >> shift, x_sub(0, cam->position[1]) >> shift,
                                x_sub(0, cam->position[2]) >> shift };
    const int32_t points = (count + 1) * ROW;
    for (int32_t i = 0; i < points; i++) {
        const int32_t t[3] = { x_add(rows.world[i][0] >> shift, offset[0]), x_add(rows.world[i][1] >> shift, offset[1]),
                               x_add(rows.world[i][2] >> shift, offset[2]) };
        mat3_apply(t, cam->matrix, rows.camera[i]);
        view_project(ctx->view, rows.camera[i], rows.screen[i]);
        if (rows.camera[i][2] < (MIRROR_CLOSE >> shift)) {   /* the mirror's: a point nearer than this is projected at twice it */
            const int32_t pushed[3] = { rows.camera[i][0], rows.camera[i][1], x_add(MIRROR_CLOSE >> shift, MIRROR_CLOSE >> shift) };
            view_project(ctx->view, pushed, rows.screen[i]);
        }
    }
    const MirrorBounds box = { ctx->clip.y1, ctx->clip.x1, ctx->clip.x0, ctx->clip.y0 };   /* bottom, right, left, top */
    for (int32_t j = 0; j < count; j++) {   /* the farthest segment first, the camera's nearest last */
        const int32_t k = ahead ? count - 1 - j : j;
        const int32_t segment = (first + k) & mask;
        const uint8_t *record = r->cache + mirror_segment_offset(segment, mask);
        const TrackLod *lod = &TRACK_LODS[rows.lod[k]];
        /* the mirror's variant is for the far; the segments nearest the camera keep the texture's fine one */
        const int32_t variant = (ahead ? k : count - 1 - k) < DETAIL_SEGMENTS ? DETAIL_VARIANT : NEAR_VARIANT;
        for (int32_t p = lod->first_polygon + lod->polygon_count - 1; p >= lod->first_polygon; p--) {
            const TrackPolygon *poly = &TRACK_POLYGONS[p];
            int32_t screen[8];
            const int32_t *corners[4];
            int behind = 0;
            for (int c = 0; c < 4; c++) {
                const int32_t v = k * ROW + poly->corner[c];
                if (v >= points) { behind = 1; break; }
                screen[2 * c] = rows.screen[v][0], screen[2 * c + 1] = rows.screen[v][1];
                corners[c] = rows.camera[v];
            }
            if (behind) continue;
            int32_t near_z = corners[0][2], far_z = corners[0][2];
            for (int c = 1; c < 4; c++) {
                if (corners[c][2] < near_z) near_z = corners[c][2];
                if (corners[c][2] > far_z) far_z = corners[c][2];
            }
            const int32_t plane = near > 0 ? near : MIRROR_CLOSE >> shift;
            int32_t clipped[4][3], clipped_screen[8];
            if (near_z < plane) {
                if (ctx->sink.draw_camera != NULL) {   /* port: the sink clips a cell to the near plane itself */
                    if (far_z <= 0) continue;
                } else {
                    if (!clip_quad(corners, plane, clipped)) continue;
                    for (int c = 0; c < 4; c++) {
                        view_project(ctx->view, clipped[c], &clipped_screen[2 * c]);
                        corners[c] = clipped[c];
                    }
                    memcpy(screen, clipped_screen, sizeof screen);
                    if (!mirror_quad_visible(&box, screen)) continue;
                }
            } else if (!mirror_quad_visible(&box, screen)) {
                continue;
            }
            const TrackUv *uv = &TRACK_UVS[poly->uv];
            const FshEntry *t = arm_texture(r->art, record[RECORD_TEXTURES + poly->strip] * FLAT_VARIANTS + uv->variant + variant);
            if (t != NULL) mirror_draw_subdivided(ctx, t, uv->u_level, 0, uv->u_cell, 0, corners, screen, near);
        }
    }
}
