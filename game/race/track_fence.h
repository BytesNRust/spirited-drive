/* A fence along one road edge of a row, as the original raises it from the
 * edge's two points and textures it.
 *
 *   track_fence_draw  0044a2b4 */
#ifndef TNFS_TRACK_FENCE_H
#define TNFS_TRACK_FENCE_H

#include "formats/fsh.h"
#include "game/race/track_draw.h"
#include "game/race/track_tables.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    const FshEntry *(*texture)(void *ctx, int32_t flat);  /* 0050eec4[flat], NULL for none */
    /* 0044a1c4: a world point to camera space and the screen */
    void (*point_screen)(void *ctx, const int32_t world[3], int32_t camera[3], int32_t screen[2]);
    /* 00425718 at depth 0 and v cell 0 */
    void (*subdivide)(void *ctx, const FshEntry *t, int32_t u_level, int32_t u_cell, const int32_t *const cam[4],
                      const int32_t screen[8], int32_t near);
    void (*full_alt)(void *ctx, const FshEntry *t, const int32_t screen[8]);  /* [004f9e90] */
    /* port: Modern 3D's, in place of full_alt when set (NULL the exe's): the quad's camera-space corners, in the
     * screen corners' order (b top, a top, a, b) */
    void (*full_camera)(void *ctx, const FshEntry *t, const int32_t *const cam[4]);
} TrackFenceOps;

typedef struct {
    const int32_t (*world)[3];   /* 0050503c */
    const int32_t (*camera)[3];  /* 00506860 */
    const int32_t (*screen)[2];  /* 00508084 */
    int32_t near;                /* *(00511fe8) +4 */
    ClipRect clip;               /* 004c78bc */
    const uint8_t *light;        /* port (PS1 light): the intensity of each point, parallel to `world`; NULL none */
    uint8_t *light_now;          /* port: 5 bytes: the corners of the polygon about to be drawn, then 1 (the draw reads them) */
} TrackFenceInputs;

/* port (PS1 light): the corners of the polygon about to be drawn (`corner`: its four vertices' offsets from `first`) in
 * light_now for the glue to hand to the sink, and cleared after it. Nothing without light. */
static inline void track_light_set(const TrackFenceInputs *in, int32_t first, const uint8_t corner[4])
{
    if (in->light == NULL || in->light_now == NULL) return;
    for (int k = 0; k < 4; k++) in->light_now[k] = in->light[first + corner[k]];
    in->light_now[4] = 1;
}

/* the same for a fence standing on the points a and b of `first`'s row (its top has their light) */
static inline void track_light_set_points(const TrackFenceInputs *in, int32_t a, int32_t b)
{
    if (in->light == NULL || in->light_now == NULL) return;
    in->light_now[0] = in->light_now[3] = in->light[b];
    in->light_now[1] = in->light_now[2] = in->light[a];
    in->light_now[4] = 1;
}

static inline void track_light_clear(const TrackFenceInputs *in)
{
    if (in->light_now != NULL) in->light_now[4] = 0;
}

/* 0044a2b4 (watcall pair, -, base, uv; variant, texture, tall; ret 0xc):
 * points a = base + pair[0], b = base + pair[1]; nothing when b is nearer
 * than `near`. Their tops (world y + 0x20000, or 0xe666 unless tall) to the
 * screen (b's first); the quad b top, a top, a, b; nothing when its points
 * share an outcode (00487b0e) or texture [uv variant + variant + texture x
 * 8] is none. Subdivided (uv's u level and cell, the tops' and the points'
 * camera corners) when the uv's variant is 0, else drawn whole. */
void track_fence_draw(const TrackFenceOps *ops, const TrackFenceInputs *in, const uint8_t pair[2], int32_t base,
                      const TrackUv *uv, int32_t variant, int32_t texture, int tall);

#endif
