/* The track editor's operations on a run of control points (dep87 1b): paste a copied block, mirror a stretch left/right or front/back,
 * reverse it. Each returns a new array and leaves its input alone; the points after the stretch (the tail) follow it rigidly, keeping the
 * turn they had where they joined it, so the road never jumps. A heading is a chord: the one arriving at a point, at point 0 the one
 * leaving it (a road's end point has nothing else). Pure C, no window; the profile keys are key_ops.h's. */
#ifndef TNFS_TRACK_EDITOR_POINT_OPS_H
#define TNFS_TRACK_EDITOR_POINT_OPS_H

#include "formats/track_trace.h"

/* the road's heading at point i as a unit vector ((0, 0) when there is no chord: fewer than 2 points, or two on top of each other) */
TracePoint pts_heading(const TracePoint *p, int n, int i);

/* p with block[1..m) put in after p[anchor]: block[0] lands on p[anchor] (it is not put in twice), the block turned so its first chord
 * continues the heading at the anchor; the tail p(anchor..n) follows from the block's last point. A new array of n + m - 1 points,
 * NULL when memory is out or the arguments are not a block and a point of p. */
TracePoint *pts_paste(const TracePoint *p, int n, int anchor, const TracePoint *block, int m);

/* p with p[s..e] reflected about the line through p[s] along the heading at s (left/right), or about the line through p[s] square to it
 * (front/back); the tail follows. A new array of n points (NULL when memory is out or s..e is not a run of p). */
TracePoint *pts_mirror_lr(const TracePoint *p, int n, int s, int e);
TracePoint *pts_mirror_fb(const TracePoint *p, int n, int s, int e);

/* p with p[s..e] travelled the other way. The whole road (0..n-1): the order turned round, nothing moves. A stretch with a road before it
 * (s > 0): its new first point sits where the old first was and continues the heading there, the tail follows. A stretch at the start
 * (s == 0): its new last point sits where the old last was and arrives there with the heading the road had arriving there; the tail stays.
 * A new array of n points, NULL as above. */
TracePoint *pts_reverse(const TracePoint *p, int n, int s, int e);

#endif
