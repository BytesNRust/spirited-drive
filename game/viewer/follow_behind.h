/* port (port_follow_camera, phase 2): the road behind the camera's node as the exe draws the road ahead of it, for a
 * camera that looks back along the track: the segments' own levels of detail, the road far to near, and per row band
 * the guard rails, the scenery objects (and in the steps after, the cars and the skid marks), all through the track
 * renderer's own draws (track_fence_draw, track_object_draw, the sink's polygons).
 *
 * The exe builds its vertex list and places its objects for the segments ahead only, and walks them backwards. This
 * builds the same lists for the segments behind (track_build_segment on a build of its own, the levels of detail by the
 * distance behind the camera) and walks them the other way, the lowest node first. The road runs toward the camera, so the
 * near plane pushes along the road direction reversed, and the rails take the pushed points (the exe drops a rail whose
 * near end is in front of the plane). */
#ifndef TNFS_FOLLOW_BEHIND_H
#define TNFS_FOLLOW_BEHIND_H

#include "game/viewer/follow_arm.h"

#include <stdint.h>

/* The track from `behind` segments before the camera's (at most FOLLOW_ARM_MAX) to `ahead` after it, the farthest
 * first: in place of the exe's pass, for a camera that does not look along the road. */
void follow_behind_draw(const FollowArm *a, int32_t behind, int32_t ahead);

/* The place of a scenery record's `node` in the run of the arm's range, which starts at `first_node` and may cross the lap seam: its distance
 * on from there round the ring (node_mask the ring's nodes less 1), so the run's order is the key's. */
int32_t follow_behind_run_key(int32_t node, int32_t first_node, int32_t node_mask);
/* the first node of the segment before the camera's: on a circuit the ring's (the walk masks it), on a open track the camera's own at the start (no segment before it; -4 was an index before the nodes) */
int32_t follow_behind_lower_node(int32_t camera_segment, int circuit);

/* The scenery records the last pass's range holds, and how many of them it drew (for the tests). */
void follow_behind_runs(int32_t *runs, int32_t *drawn);

/* The road direction in camera space pointing away from the camera, from the world positions of two nodes, `lo` the one
 * nearer the track's start: (lo - hi) >> shift through the matrix, its z at least 0x1999 as 004497d8 has it. */
void follow_behind_direction(const int32_t lo[3], const int32_t hi[3], const int32_t matrix[9], int shift, int32_t dir[3]);

#endif
