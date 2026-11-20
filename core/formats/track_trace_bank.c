/* The road's bank from the road keys (track_trace.h): the values checked, set on the nodes along the arc length, read at the control points, and the key
 * list's edits for it. track_trace.c hands over the arc length of each control point (it owns the curve). */
#include "formats/track_trace_bank.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int trace_bank_check(int point_count, const TraceKey *keys, int key_count, ExchangeError *err)
{
    for (int k = 0; k < key_count; ++k) {
        if (!(keys[k].has & TRACE_KEY_BANK)) continue;
        const int point = keys[k].point;
        if (point < 0 || point >= point_count) return exchange_fail(err, "a road key is on point %d, and the trace has points 0..%d", point, point_count - 1), 0;
        if (!(fabs(keys[k].bank_deg) <= TRACE_BANK_INPUT_MAX_DEG))   /* NaN too */
            return exchange_fail(err, "the road key on point %d banks %g degrees, outside -%g..%g", point, keys[k].bank_deg, TRACE_BANK_INPUT_MAX_DEG,
                                 TRACE_BANK_INPUT_MAX_DEG), 0;
    }
    return 1;
}

typedef struct {
    double d, v;   /* arc length of the key's point; its bank */
} Stop;

static double smoothstep(double t)
{
    return t * t * (3 - 2 * t);
}

/* the bank's stops, ascending by arc length, with a level start at 0 when the first key is further on; how many (0: no key banks) */
static int stops_of(const double *point_s, const TraceKey *keys, int key_count, Stop *stops)
{
    int n = 0;
    for (int k = 0; k < key_count; ++k) {
        if (!(keys[k].has & TRACE_KEY_BANK)) continue;
        const Stop stop = { point_s[keys[k].point], keys[k].bank_deg };
        int at = n++;
        while (at > 0 && stops[at - 1].d > stop.d) {
            stops[at] = stops[at - 1];
            --at;
        }
        stops[at] = stop;
    }
    if (n > 0 && stops[0].d > 0) {
        memmove(stops + 1, stops, sizeof *stops * (size_t)n++);
        stops[0].d = 0;
        stops[0].v = 0;
    }
    return n;
}

/* the bank at arc length `at`: a smoothstep between the stops around it, the last one's after it */
static double bank_at(const Stop *stops, int n, double at)
{
    for (int j = 0; j + 1 < n; ++j) {
        if (at >= stops[j + 1].d) continue;
        const double span = stops[j + 1].d - stops[j].d, t = span > 0 ? (at - stops[j].d) / span : 1;
        return stops[j].v + (stops[j + 1].v - stops[j].v) * smoothstep(t < 0 ? 0 : t);
    }
    return stops[n - 1].v;
}

int trace_bank_apply(const double *point_s, const TraceKey *keys, int key_count, double step, TrackGenNode *nodes, int node_count, ExchangeError *err)
{
    Stop *stops = malloc(sizeof *stops * (size_t)(key_count + 1));
    if (stops == NULL) return exchange_fail(err, "out of memory"), 0;
    const int n = stops_of(point_s, keys, key_count, stops);
    for (int k = 0; n > 0 && k < node_count; ++k) nodes[k].bank = bank_at(stops, n, k * step);
    free(stops);
    return 1;
}

void trace_bank_at_points(const double *point_s, int point_count, const TraceKey *keys, int key_count, double *degrees)
{
    for (int i = 0; i < point_count; ++i) degrees[i] = 0;
    Stop *stops = point_s != NULL ? malloc(sizeof *stops * (size_t)(key_count + 1)) : NULL;
    const int n = stops != NULL ? stops_of(point_s, keys, key_count, stops) : 0;
    for (int i = 0; i < point_count && n > 0; ++i) degrees[i] = bank_at(stops, n, point_s[i]);
    free(stops);
}

/* ---- the key list's edits ---- */

int track_trace_key_set_bank(TraceKey **keys, int *count, int *cap, int point, double degrees)
{
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
        memset(&(*keys)[at], 0, sizeof **keys);   /* every other value of a new key unset */
        (*keys)[at].point = point;
        ++*count;
    }
    (*keys)[at].has |= TRACE_KEY_BANK;
    (*keys)[at].bank_deg = degrees;
    return 1;
}

void track_trace_key_clear_bank(TraceKey *keys, int *count, int point)
{
    const int at = track_trace_key_find(keys, *count, point);
    if (at < 0) return;
    keys[at].has &= ~(unsigned)TRACE_KEY_BANK;
    keys[at].bank_deg = 0;
    if (keys[at].has == 0) {
        for (int i = at; i + 1 < *count; ++i) keys[i] = keys[i + 1];
        --*count;
    }
}
