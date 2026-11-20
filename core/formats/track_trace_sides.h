/* The road's sides from the road keys (step 3), for track_trace.c: the public side of it is track_trace.h. */
#ifndef TNFS_FORMATS_TRACK_TRACE_SIDES_H
#define TNFS_FORMATS_TRACK_TRACE_SIDES_H

#include "formats/track_trace.h"

/* the keys' points are 0..point_count-1, once each, and every side value they set is in its range; 0 with err naming the point and the value */
int trace_sides_check(int point_count, const TraceKey *keys, int key_count, ExchangeError *err);

/* sets the nodes' sides from the keys: node k is at arc length k * step, point i at point_s[i]. The keys are checked (trace_sides_check). 0 when out of
 * memory. */
int trace_sides_apply(const double *point_s, const TraceKey *keys, int key_count, double step, TrackGenNode *nodes, int node_count, ExchangeError *err);

/* each point's values (track_trace_point_sides); point_s NULL: the template's for all */
void trace_sides_at_points(const double *point_s, int point_count, const TraceKey *keys, int key_count, double (*values)[TRACE_SIDE_FIELDS][2]);

#endif
