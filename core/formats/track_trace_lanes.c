/* Lanes from the road keys (dep59 editor phase 2, step 4): see track_trace_lanes.h and track_trace.h. */
#include "formats/track_trace_lanes.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { NODES_PER_SEGMENT = 4 };

static int lanes_ok(int lanes)
{
    return lanes == TRACK_GEN_LANES_1_1 || lanes == TRACK_GEN_LANES_1_2 || lanes == TRACK_GEN_LANES_2_1;
}

static int is_stretch(int lanes) { return lanes == TRACK_GEN_LANES_1_2 || lanes == TRACK_GEN_LANES_2_1; }

int trace_lanes_check(const TraceKey *keys, int key_count, ExchangeError *err)
{
    for (int k = 0; k < key_count; ++k)
        if ((keys[k].has & TRACE_KEY_LANES) && !lanes_ok(keys[k].lanes))
            return exchange_fail(err, "the road key on point %d has lanes 0x%02x: 1+1, 1+2 or 2+1", keys[k].point, (unsigned)keys[k].lanes), 0;
    return 1;
}

void track_trace_point_lanes(const TraceKey *keys, int key_count, int point_count, int *lanes)
{
    int now = TRACK_GEN_LANES_1_1, k = 0;
    for (int i = 0; i < point_count; ++i) {
        while (k < key_count && keys[k].point < i) ++k;
        if (k < key_count && keys[k].point == i && (keys[k].has & TRACE_KEY_LANES) && lanes_ok(keys[k].lanes)) now = keys[k].lanes;
        lanes[i] = now;
    }
}

/* each point's lanes, malloc'd; NULL when out of memory */
static int *point_lanes_of(const TraceKey *keys, int key_count, int point_count)
{
    int *lanes = malloc(sizeof *lanes * (size_t)(point_count > 0 ? point_count : 1));
    if (lanes != NULL) track_trace_point_lanes(keys, key_count, point_count, lanes);
    return lanes;
}

static int ignored(const TraceKey *k, const int *lanes, int point_count)
{
    return (k->has & TRACE_KEY_WIDTH) && k->point >= 0 && k->point < point_count && is_stretch(lanes[k->point]);
}

int track_trace_lane_ignored_widths(const TracePoint *points, int count, const TraceKey *keys, int key_count, int *out, int cap)
{
    (void)points;
    int *lanes = point_lanes_of(keys, key_count, count);
    if (lanes == NULL) return 0;
    int n = 0;
    for (int k = 0; k < key_count; ++k)
        if (ignored(&keys[k], lanes, count)) {
            if (out != NULL && n < cap) out[n] = keys[k].point;
            ++n;
        }
    free(lanes);
    return n;
}

TraceKey *trace_lanes_width_keys(const TraceKey *keys, int key_count, int point_count)
{
    TraceKey *copy = malloc(sizeof *copy * (size_t)(key_count > 0 ? key_count : 1));
    int *lanes = point_lanes_of(keys, key_count, point_count);
    if (copy == NULL || lanes == NULL) {
        free(copy);
        free(lanes);
        return NULL;
    }
    if (key_count > 0) memcpy(copy, keys, sizeof *copy * (size_t)key_count);
    for (int k = 0; k < key_count; ++k)
        if (ignored(&copy[k], lanes, point_count)) copy[k].has &= ~TRACE_KEY_WIDTH;
    free(lanes);
    return copy;
}

static double toward(double from, double to, double most) { return to > from + most ? from + most : to < from - most ? from - most : to; }

int trace_lanes_apply(const double *point_s, int point_count, const TraceKey *keys, int key_count, double step, TrackGenNode *nodes, int node_count)
{
    int any = 0;
    for (int k = 0; k < key_count; ++k) any |= (keys[k].has & TRACE_KEY_LANES) && is_stretch(keys[k].lanes);
    if (!any) return 1;   /* the road of before, node for node */
    int *lanes = point_lanes_of(keys, key_count, point_count);
    if (lanes == NULL) return 0;
    double left = 0, right = 0, was = 0;   /* the halves of the node before as written, and its width from the width keys */
    int point = 0;
    for (int k = 0; k < node_count; ++k) {
        const double at = (k - k % NODES_PER_SEGMENT) * step;   /* the segment's start: its pictures go by segment */
        while (point + 1 < point_count && point_s[point + 1] <= at + 1e-9) ++point;
        const int l = lanes[point];
        const double own = nodes[k].width;
        double want_left = own, want_right = own;
        if (l == TRACK_GEN_LANES_1_2) want_left = TRACK_GEN_LANE, want_right = 2 * TRACK_GEN_LANE;
        if (l == TRACK_GEN_LANES_2_1) want_left = 2 * TRACK_GEN_LANE, want_right = TRACK_GEN_LANE;
        const int easing = k > 0 && (is_stretch(l) || left != was || right != was);
        if (easing) {   /* in a stretch, or still on the way out of one */
            want_left = toward(left, want_left, TRACE_LANE_STEP);
            want_right = toward(right, want_right, TRACE_LANE_STEP);
        }
        if (k % NODES_PER_SEGMENT == 0) {   /* the segment before draws its picture up to this row: a two-lane half narrows from the next node */
            if (left >= 2 * TRACK_GEN_LANE && want_left < left) want_left = left;
            if (right >= 2 * TRACK_GEN_LANE && want_right < right) want_right = right;
        }
        if (l == TRACK_GEN_LANES_1_2 && want_right < TRACK_GEN_LANE) want_right = TRACK_GEN_LANE;   /* never a lane under 5 m: a jump at the start */
        if (l == TRACK_GEN_LANES_2_1 && want_left < TRACK_GEN_LANE) want_left = TRACK_GEN_LANE;
        left = want_left, right = want_right, was = own;
        nodes[k].lanes = is_stretch(l) ? l : 0;
        nodes[k].width = left;
        nodes[k].width_r = right != left ? right : 0;
    }
    free(lanes);
    return 1;
}

int track_trace_key_set_lanes(TraceKey **keys, int *count, int *cap, int point, int lanes)
{
    if (!lanes_ok(lanes)) return 0;
    int at = track_trace_key_find(*keys, *count, point);
    if (at < 0) {
        if (*count == *cap) {
            const int grown = *cap == 0 ? 8 : *cap * 2;
            TraceKey *more = realloc(*keys, sizeof *more * (size_t)grown);
            if (more == NULL) return 0;
            *keys = more;
            *cap = grown;
        }
        at = *count;
        while (at > 0 && (*keys)[at - 1].point > point) {
            (*keys)[at] = (*keys)[at - 1];
            --at;
        }
        memset(&(*keys)[at], 0, sizeof **keys);
        (*keys)[at].point = point;
        ++*count;
    }
    (*keys)[at].has |= TRACE_KEY_LANES;
    (*keys)[at].lanes = lanes;
    return 1;
}

void track_trace_key_clear_lanes(TraceKey *keys, int *count, int point)
{
    const int at = track_trace_key_find(keys, *count, point);
    if (at < 0) return;
    keys[at].has &= ~TRACE_KEY_LANES;
    keys[at].lanes = 0;
    if (keys[at].has != 0) return;
    memmove(&keys[at], &keys[at + 1], sizeof *keys * (size_t)(*count - at - 1));
    --*count;
}
