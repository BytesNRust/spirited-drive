/* The near segments, drawn far to near after the far road: per segment
 * its LOD's polygons last to first, culled against the camera's box and
 * subdivided; at the end of each row band the fences, then per row the
 * objects on its node, the skid marks, the cars on the node after it and
 * the flat objects left on the next node.
 *
 *   track_near_draw  0044b510 */
#ifndef TNFS_TRACK_NEAR_H
#define TNFS_TRACK_NEAR_H

#include "game/race/mirror_scene.h"
#include "game/race/race_view.h"
#include "game/race/track_far.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    /* 00459eb8 (-999, node, row, 0; s, t): the skid marks about road node
     * `row`, kept to the side of s -> t */
    void (*skids)(void *ctx, int32_t node, int32_t row, const int32_t s[2], const int32_t t[2]);
    /* 0044b0c8 (segments, node, near): the far road's other draw, taken
     * while 00511fbc is set */
    void (*far_alternate)(void *ctx, int32_t segments, int32_t node, int32_t near);
} TrackNearOps;

typedef struct {
    const TrackFarInputs *far;   /* the segments, the road points, the walk and the fences, as 0044ad5c's */
    const ViewLayout *layout;    /* *(00511fe8) +0x1c */
    int32_t alternate;           /* 00511fbc */
    MirrorBounds *bounds;        /* 00511f88..00511f94 */
} TrackNearInputs;

/* 0044b510 (watcall segments, node): near = segments, at most 4; the
 * camera's box (00449f50); the far road (0044ad5c) unless `alternate`.
 * From the last near segment (node / 4 + near - 1, masked) back to the
 * camera's: the points before the segment's (from 00509780 - 11 - 11 per
 * far segment, back by each segment's count, 0050422c), its polygons last
 * to first: when 00449f90 passes, subdivided (00425718: uv level and cell,
 * near *(00511fe8)+4) with texture [record +2 + strip] x 8 + uv variant +
 * 004c2ea8[band]; a polygon ending a row band: its fences as 0044ad5c's,
 * then for each of its rows the objects on the row's node (0044a4b0), the
 * skids (its corners 0 and 3), the cars on the node after it, the node one
 * back and the objects on it whose record +8 is 0. Then the cars on the
 * node after the last row, and 0044b0c8 when `alternate`. */
void track_near_draw(const TrackFarOps *far_ops, const TrackNearOps *ops, const TrackNearInputs *in, int32_t segments,
                     int32_t node);

#endif
