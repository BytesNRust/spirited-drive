/* The road's sides from the road keys (step 3; track_trace.h): the values checked, set on the nodes along the arc length, read at the control points, and
 * the key list's edits for them. track_trace.c hands over the arc length of each control point (it owns the curve). */
#include "formats/track_trace_sides.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *const SIDE_NAME[2] = { "left", "right" };

static int is_width(int field)
{
    return field == TRACE_SIDE_SHOULDER || field == TRACE_SIDE_BARRIER;
}

static double side_default(int field)
{
    return field == TRACE_SIDE_SHOULDER ? TRACE_SHOULDER_DEFAULT_M : field == TRACE_SIDE_BARRIER ? TRACE_BARRIER_DEFAULT_M
         : field == TRACE_SIDE_WALL ? TRACK_GEN_WALL_SOFT : 1;
}

static int value_ok(int field, double v)
{
    if (is_width(field)) return v >= 0 && v <= TRACE_SIDE_MAX_M;
    if (field == TRACE_SIDE_WALL) return v == TRACK_GEN_WALL_HARD || v == TRACK_GEN_WALL_SOFT;
    return v == 0 || v == 1;
}

static int fault(ExchangeError *err, int point, int field, int side, double v)
{
    const char *s = SIDE_NAME[side];
    if (field == TRACE_SIDE_SHOULDER) return exchange_fail(err, "the road key on point %d has a %s shoulder of %g m, outside 0..%g m", point, s, v, TRACE_SIDE_MAX_M), 0;
    if (field == TRACE_SIDE_BARRIER)
        return exchange_fail(err, "the road key on point %d has a %s wall %g m from the road edge, outside 0..%g m", point, s, v, TRACE_SIDE_MAX_M), 0;
    if (field == TRACE_SIDE_WALL) return exchange_fail(err, "the road key on point %d has a %s wall kind of %g: hard (0) or soft (1)", point, s, v), 0;
    return exchange_fail(err, "the road key on point %d has a %s fence of %g: on (1) or off (0)", point, s, v), 0;
}

int trace_sides_check(int point_count, const TraceKey *keys, int key_count, ExchangeError *err)
{
    for (int k = 0; k < key_count; ++k) {
        const int point = keys[k].point;
        if (point < 0 || point >= point_count) return exchange_fail(err, "a road key is on point %d, and the trace has points 0..%d", point, point_count - 1), 0;
        for (int j = 0; j < k; ++j)
            if (keys[j].point == point) return exchange_fail(err, "point %d has a road key twice", point), 0;
        for (int f = 0; f < TRACE_SIDE_FIELDS; ++f)
            for (int s = 0; s < 2; ++s)
                if ((keys[k].has & TRACE_KEY_SIDE(f, s)) && !value_ok(f, keys[k].side[f][s])) return fault(err, point, f, s, keys[k].side[f][s]);
    }
    return 1;
}

typedef struct {
    double d, v;   /* arc length of the key's point; its value (metres, or the kind) */
} Stop;

static double smoothstep(double t)
{
    return t * t * (3 - 2 * t);
}

/* the stops of one value of one side, ascending by arc length: a width's start at 0 with the template's when the first key is further on. How many. */
static int stops_of(const double *point_s, const TraceKey *keys, int key_count, int field, int side, Stop *stops)
{
    int n = 0;
    for (int k = 0; k < key_count; ++k) {
        if (!(keys[k].has & TRACE_KEY_SIDE(field, side))) continue;
        const Stop stop = { point_s[keys[k].point], keys[k].side[field][side] };
        int at = n++;
        while (at > 0 && stops[at - 1].d > stop.d) {
            stops[at] = stops[at - 1];
            --at;
        }
        stops[at] = stop;
    }
    if (n > 0 && is_width(field) && stops[0].d > 0) {
        memmove(stops + 1, stops, sizeof *stops * (size_t)n++);
        stops[0].d = 0;
        stops[0].v = side_default(field);
    }
    return n;
}

/* the value at arc length `at`: a width by a smoothstep between the stops (held after the last); a kind the last stop's at or before it. 0 when a kind has
 * no stop there yet (the template's). */
static int value_at(const Stop *stops, int n, int field, double at, double *v)
{
    if (n == 0) return 0;
    if (!is_width(field)) {
        int last = -1;
        for (int j = 0; j < n; ++j)
            if (stops[j].d <= at + 1e-9) last = j;
        if (last < 0) return 0;
        *v = stops[last].v;
        return 1;
    }
    *v = stops[n - 1].v;
    for (int j = 0; j + 1 < n; ++j) {
        if (at >= stops[j + 1].d) continue;
        const double span = stops[j + 1].d - stops[j].d, t = span > 0 ? (at - stops[j].d) / span : 1;
        *v = stops[j].v + (stops[j + 1].v - stops[j].v) * smoothstep(t < 0 ? 0 : t);
        break;
    }
    return 1;
}

static const unsigned GEN_BIT[TRACE_SIDE_FIELDS] = { TRACK_GEN_SIDE_SHOULDER, TRACK_GEN_SIDE_BARRIER, TRACK_GEN_SIDE_WALL, TRACK_GEN_SIDE_FENCE };

int trace_sides_apply(const double *point_s, const TraceKey *keys, int key_count, double step, TrackGenNode *nodes, int node_count, ExchangeError *err)
{
    Stop *stops = malloc(sizeof *stops * (size_t)(key_count + 1));
    if (stops == NULL) return exchange_fail(err, "out of memory"), 0;
    for (int f = 0; f < TRACE_SIDE_FIELDS; ++f)
        for (int s = 0; s < 2; ++s) {
            const int n = stops_of(point_s, keys, key_count, f, s, stops);
            for (int k = 0; n > 0 && k < node_count; ++k) {
                double v;
                if (!value_at(stops, n, f, k * step, &v)) continue;
                TrackGenSide *side = &nodes[k].side[s];
                side->has |= GEN_BIT[f];
                if (f == TRACE_SIDE_SHOULDER) side->shoulder = v * 8;   /* the verge byte's 1/8 m */
                else if (f == TRACE_SIDE_BARRIER) side->barrier = v * 8;
                else if (f == TRACE_SIDE_WALL) side->wall = (int)v;
                else side->fence = (int)v;
            }
        }
    free(stops);
    return 1;
}

void trace_sides_at_points(const double *point_s, int point_count, const TraceKey *keys, int key_count, double (*values)[TRACE_SIDE_FIELDS][2])
{
    Stop *stops = malloc(sizeof *stops * (size_t)(key_count + 1));
    for (int i = 0; i < point_count; ++i)
        for (int f = 0; f < TRACE_SIDE_FIELDS; ++f)
            for (int s = 0; s < 2; ++s) values[i][f][s] = side_default(f);
    if (stops == NULL || point_s == NULL) {
        free(stops);
        return;
    }
    for (int f = 0; f < TRACE_SIDE_FIELDS; ++f)
        for (int s = 0; s < 2; ++s) {
            const int n = stops_of(point_s, keys, key_count, f, s, stops);
            for (int i = 0; i < point_count; ++i) value_at(stops, n, f, point_s[i], &values[i][f][s]);
        }
    free(stops);
}

/* ---- the key list's edits ---- */

int track_trace_key_set_side(TraceKey **keys, int *count, int *cap, int point, int field, int side, double value)
{
    if (field < 0 || field >= TRACE_SIDE_FIELDS || side < 0 || side > 1) return 0;
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
    (*keys)[at].has |= TRACE_KEY_SIDE(field, side);
    (*keys)[at].side[field][side] = value;
    return 1;
}

void track_trace_key_clear_side(TraceKey *keys, int *count, int point, int field, int side)
{
    const int at = track_trace_key_find(keys, *count, point);
    if (at < 0 || field < 0 || field >= TRACE_SIDE_FIELDS || side < 0 || side > 1) return;
    keys[at].has &= ~TRACE_KEY_SIDE(field, side);
    keys[at].side[field][side] = 0;
    if (keys[at].has == 0) {
        for (int i = at; i + 1 < *count; ++i) keys[i] = keys[i + 1];
        --*count;
    }
}
