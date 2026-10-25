/* port (port_follow_camera): the road and terrain behind the camera's node, for a camera that looks back along the track.
 * The exe draws the segments ahead of the camera's node only (track_far_draw, track_near_draw) and a short row of
 * five behind for the rear mirror (mirror_scene.c), in one low level of detail. This draws the same row-of-points,
 * low-LOD scene as the mirror's, over more segments, in the main view: the first row of each segment from the camera's
 * segment back, the LOD's polygons far to near, no scenery objects, cars or skid marks. */
#ifndef TNFS_FOLLOW_ARM_H
#define TNFS_FOLLOW_ARM_H

#include "game/race/track_draw.h"
#include "game/race/track_render.h"

#include <stdint.h>

enum { FOLLOW_ARM_SEGMENTS = 10 };   /* how far behind the camera's segment (the cache holds 62 segments, 30 of them ahead) */

typedef struct {
    const TrackRender *render;   /* the cache, nodes and masks, bound to the track */
    const TrackCamera *camera;   /* position, matrix and node */
    const DrawContext *ctx;      /* the view, clip and sink of the main view's draws */
    int circuit;
} FollowArm;

enum { FOLLOW_ARM_BEHIND, FOLLOW_ARM_AHEAD };

/* The arm drawn into the sink's target (its draws are the main view's, so recorded for the bands and the render scale's
 * as the track's are). `segments` at most FOLLOW_ARM_SEGMENTS. The behind arm is the segments before the camera's, the ahead
 * one, for the mirror of a car that points back along the track, the camera's and those after it, the farthest first. */
void follow_arm_draw(const FollowArm *a, int32_t segments, int ahead);

/* The cache window's reach behind, for an arm of `segments`: the segments loaded again after the camera jumps. */
void follow_arm_window(TrackRender *r, int32_t segments);

#endif
