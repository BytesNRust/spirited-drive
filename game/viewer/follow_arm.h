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

enum {
    FOLLOW_ARM_SEGMENTS = 10,        /* how far behind the camera's segment with the exe's ring (the cache holds 62 segments, 30 of them ahead) */
    FOLLOW_ARM_RING_SEGMENTS = 30,   /* and with port_follow_ring's bigger one (track_cache.h) */
    FOLLOW_ARM_MAX = FOLLOW_ARM_RING_SEGMENTS
};

/* how far behind the camera's segment the arm goes: FOLLOW_ARM_SEGMENTS, or FOLLOW_ARM_RING_SEGMENTS when the cache's ring is the bigger one */
int32_t follow_arm_segments(void);

typedef struct {
    const TrackRender *render;   /* the cache, nodes and masks, bound to the track */
    const TrackCamera *camera;   /* position, matrix and node */
    const DrawContext *ctx;      /* the view, clip and sink of the main view's draws */
    int circuit;
    int32_t frame;               /* the scenery's animation counter (00513e70), for the road behind (follow_behind.h) */
    int mirrored;                /* a mirrored track (004fbad8) */
    int low_lod;                 /* the rear mirror's: no segment at a finer level than the far one (follow_behind.h) */
} FollowArm;

enum { FOLLOW_ARM_BEHIND, FOLLOW_ARM_AHEAD };

/* The arm drawn into the sink's target (its draws are the main view's, so recorded for the bands and the render scale's
 * as the track's are). `segments` at most FOLLOW_ARM_MAX. The behind arm is the segments before the camera's, the ahead
 * one, for the mirror of a car that points back along the track, the camera's and those after it, the farthest first. */
void follow_arm_draw(const FollowArm *a, int32_t segments, int ahead);

/* port, the classic sink: a quad's corners nearer than `plane` slid along its edges to it, toward a corner in front (an
 * adjacent one, else the opposite; two adjacent ones are averaged), so a quad across the camera still projects as a quad.
 * 0 when no corner is in front. */
int follow_arm_clip_quad(const int32_t *const in[4], int32_t plane, int32_t out[4][3]);

/* How many of the `wanted` segments behind the camera's the cache really keeps: what its window holds behind its ahead (`behind` - `ahead`), at
 * least 0. The cache is a ring of 64 records; an Ultra view's 58 ahead leave the exe's 5 behind (follow_arm_window cannot raise it), and a segment
 * farther behind reads the record of one at the far end ahead: a polygon from one end of the drawn stretch to the other. */
int32_t follow_arm_kept_behind(const TrackCacheWindow *w, int32_t wanted);

/* The cache window's reach behind, for an arm of `segments`: the segments loaded again after the camera jumps. */
void follow_arm_window(TrackRender *r, int32_t segments);

#endif
