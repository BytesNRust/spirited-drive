/* Lanes from the road keys (step 4), for track_trace.c: the public side of it is track_trace.h. */
#ifndef TNFS_FORMATS_TRACK_TRACE_LANES_H
#define TNFS_FORMATS_TRACK_TRACE_LANES_H

#include "formats/track_trace.h"

/* every key's lanes is 1+1, 1+2 or 2+1; 0 with err naming the point */
int trace_lanes_check(const TraceKey *keys, int key_count, ExchangeError *err);

/* a copy of the keys whose width a lane stretch fixes (a width key on a point that has 1+2 or 2+1) has its width bit cleared: the keys the width
 * comes from. Malloc'd (key_count, at least one), NULL when out of memory. */
TraceKey *trace_lanes_width_keys(const TraceKey *keys, int key_count, int point_count);

/* sets the nodes' lanes and halves: node k's segment takes the lanes of the last point at or before its start (point i at point_s[i], node k at
 * k * step); in a 1+2 or 2+1 stretch the halves are the lanes' (TRACK_GEN_LANE each), into and out of it each half moves at most
 * TRACE_LANE_STEP a node and the two-lane half is never under a lane. Nothing when no key says 1+2 or 2+1. 0 when out of memory. */
int trace_lanes_apply(const double *point_s, int point_count, const TraceKey *keys, int key_count, double step, TrackGenNode *nodes, int node_count);

#endif
