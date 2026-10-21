/* The road beyond the near segments, drawn far to near as the original
 * does: per segment its LOD's polygons last to first, each culled and drawn
 * whole; at the end of each row band the fences, then per row the objects
 * on its node and the cars on the node after it.
 *
 *   track_far_draw  0044ad5c */
#ifndef TNFS_TRACK_FAR_H
#define TNFS_TRACK_FAR_H

#include "formats/fsh.h"
#include "game/race/track_build.h"
#include "game/race/track_fence.h"
#include "game/race/track_object_draw.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    const FshEntry *(*texture)(void *ctx, int32_t flat);                /* 0050eec4[flat], NULL for none */
    void (*full)(void *ctx, const FshEntry *t, const int32_t screen[8]);  /* [004f9e9c] */
    void (*cars)(void *ctx, int32_t node);                               /* 0045a848 */
    /* port: Modern 3D's: the polygon from its camera-space corners in place of `full`; NULL the exe's */
    void (*full_camera)(void *ctx, const FshEntry *t, const int32_t *const cam[4]);
} TrackFarOps;

typedef struct {
    const TrackSegmentEntry *segments;  /* 00504228 */
    uint32_t segment_mask;              /* 00504210 */
    const uint8_t *cache;               /* 005041d8: +1 the fence byte, +2 the 10 texture bytes */
    int32_t vertex_count;               /* 00509780 */
    int mirrored;                       /* 004fbad8 */
    int32_t camera_flag;                /* *(00511fe0) +0 */
    TrackFenceInputs fence;             /* the road points (world, camera, screen), near and clip */
    const TrackFenceOps *fence_ops;
    TrackObjectDraw *objects;           /* the walk, its node mask 00504208 the road's too */
    uint8_t *quarter;                   /* 0051204c */
} TrackFarInputs;

/* 0044ad5c (watcall segments, node, near): quarter = 1 << (node & 3),
 * << 4 more when the camera flag is set. From the last of `segments`
 * segments after the camera's (node / 4 + segments - 1) down while rows
 * remain beyond `near` x 4: the points before the segment's (from 00509780
 * - 11 back by each LOD's count), its polygons last to first: drawn whole
 * with texture [record +2 + strip] x 8 + uv variant + 004c2f20[band / 4]
 * when 0044a0d0 passes and corner 3 is in front (z > 0); a polygon ending a
 * row band: its fences (record +1: 0x80 the LOD's +0x10 pairs, 0x40 its +8,
 * by the row's node & 3; texture 0x20 + low 5 bits, tall unless 0x20),
 * then for each of its rows the objects on the row's node (0044a4b0 with
 * the band) and the cars on the node after it, the node and band one back.
 * The next segment is the row's node / 4. */
void track_far_draw(const TrackFarOps *ops, const TrackFarInputs *in, int32_t segments, int32_t node, int32_t near);

#endif
