/* The fence step from the road keys, for track_trace.c: the public side of it is track_trace.h. */
#ifndef TNFS_FORMATS_TRACK_TRACE_FENCE_H
#define TNFS_FORMATS_TRACK_TRACE_FENCE_H

#include "formats/track_trace.h"

/* every fence value the keys set is in range (height 0/1, a TRACE_FENCE_ picture, Custom 0/1); 0 with err naming the point and the value */
int trace_fence_check(const TraceKey *keys, int key_count, ExchangeError *err);

/* after trace_sides_apply: sets each node's fence look from the held keys (node k at k * step, point i at point_s[i]), and each side's barrier by
 * barrier_line where that side's Custom does not hold. 0 when out of memory. */
int trace_fence_apply(const double *point_s, const TraceKey *keys, int key_count, double step, TrackGenNode *nodes, int node_count);

#endif
