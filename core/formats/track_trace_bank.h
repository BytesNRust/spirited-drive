/* The road's bank from the road keys (dep59 phase 2), for track_trace.c: the public side of it is track_trace.h. */
#ifndef TNFS_FORMATS_TRACK_TRACE_BANK_H
#define TNFS_FORMATS_TRACK_TRACE_BANK_H

#include "formats/track_trace.h"

/* every bank a key sets is a number in +-TRACE_BANK_INPUT_MAX_DEG on a point 0..point_count-1; 0 with err naming the point */
int trace_bank_check(int point_count, const TraceKey *keys, int key_count, ExchangeError *err);

/* sets the nodes' bank from the keys: node k is at arc length k * step, point i at point_s[i]. Level where no key speaks. 0 when out of memory. */
int trace_bank_apply(const double *point_s, const TraceKey *keys, int key_count, double step, TrackGenNode *nodes, int node_count, ExchangeError *err);

/* each point's bank (track_trace_point_banks); point_s NULL: level for all */
void trace_bank_at_points(const double *point_s, int point_count, const TraceKey *keys, int key_count, double *degrees);

#endif
