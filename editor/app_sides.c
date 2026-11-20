/* The road's sides and lanes on the editor's road keys (steps 3 and 4; app.h): set and clear a side value or the lanes of a control point, as the Road
 * profile block edits them. */
#include "app.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* what a side value is kept to: widths to a quarter of a metre in 0..TRACE_SIDE_MAX_M, a wall to hard or soft, a fence to on or off */
static double side_value(int field, double v)
{
    if (field == TRACE_SIDE_WALL) return v == TRACK_GEN_WALL_HARD ? TRACK_GEN_WALL_HARD : TRACK_GEN_WALL_SOFT;
    if (field == TRACE_SIDE_FENCE) return v != 0 ? 1 : 0;
    const double m = !(v >= 0) ? 0 : v > TRACE_SIDE_MAX_M ? TRACE_SIDE_MAX_M : v;
    return round(m * 4) / 4;
}

static int side_ok(const App *a, int point, int field, int side)
{
    return a->kind == APP_KIND_TRACK && point >= 0 && point < a->trace.count && field >= 0 && field < TRACE_SIDE_FIELDS && (side == TRACK_GEN_LEFT || side == TRACK_GEN_RIGHT);
}

void app_set_point_side(App *a, int point, int field, int side, double value)
{
    if (!side_ok(a, point, field, side)) return;
    const double want = side_value(field, value);
    const int at = track_trace_key_find(a->trace.keys, a->trace.key_count, point);
    if (at >= 0 && (a->trace.keys[at].has & TRACE_KEY_SIDE(field, side)) && a->trace.keys[at].side[field][side] == want) return;   /* the same: nothing changed */
    if (!track_trace_key_set_side(&a->trace.keys, &a->trace.key_count, &a->trace.key_cap, point, field, side, want)) {
        snprintf(a->status, sizeof a->status, "out of memory");
        return;
    }
    a->dirty = 1;
}

void app_clear_point_side(App *a, int point, int field, int side)
{
    if (!side_ok(a, point, field, side)) return;
    const int at = track_trace_key_find(a->trace.keys, a->trace.key_count, point);
    if (at < 0 || !(a->trace.keys[at].has & TRACE_KEY_SIDE(field, side))) return;   /* nothing to clear */
    track_trace_key_clear_side(a->trace.keys, &a->trace.key_count, point, field, side);
    a->dirty = 1;
}

static int lanes_point_ok(const App *a, int point) { return a->kind == APP_KIND_TRACK && point >= 0 && point < a->trace.count; }

void app_set_point_lanes(App *a, int point, int lanes)
{
    if (!lanes_point_ok(a, point) || !(lanes == TRACK_GEN_LANES_1_1 || lanes == TRACK_GEN_LANES_1_2 || lanes == TRACK_GEN_LANES_2_1)) return;
    const int at = track_trace_key_find(a->trace.keys, a->trace.key_count, point);
    if (at >= 0 && (a->trace.keys[at].has & TRACE_KEY_LANES) && a->trace.keys[at].lanes == lanes) return;   /* the same: nothing changed */
    if (!track_trace_key_set_lanes(&a->trace.keys, &a->trace.key_count, &a->trace.key_cap, point, lanes)) {
        snprintf(a->status, sizeof a->status, "out of memory");
        return;
    }
    a->dirty = 1;
}

void app_clear_point_lanes(App *a, int point)
{
    if (!lanes_point_ok(a, point)) return;
    const int at = track_trace_key_find(a->trace.keys, a->trace.key_count, point);
    if (at < 0 || !(a->trace.keys[at].has & TRACE_KEY_LANES)) return;   /* nothing to clear */
    track_trace_key_clear_lanes(a->trace.keys, &a->trace.key_count, point);
    a->dirty = 1;
}

int app_point_lanes(const App *a, int point)
{
    if (!lanes_point_ok(a, point)) return 0;
    int lanes = TRACK_GEN_LANES_1_1;
    for (int k = 0; k < a->trace.key_count && a->trace.keys[k].point <= point; ++k)
        if (a->trace.keys[k].has & TRACE_KEY_LANES) lanes = a->trace.keys[k].lanes;
    return lanes;
}

int app_lane_width_note(const App *a, char *text, size_t cap)
{
    enum { NAMED = 8 };
    if (a->kind != APP_KIND_TRACK) return 0;
    int points[NAMED];
    const int n = track_trace_lane_ignored_widths(a->trace.points, a->trace.count, a->trace.keys, a->trace.key_count, points, NAMED);
    if (n == 0) return 0;
    size_t used = (size_t)snprintf(text, cap, "the width on point%s", n > 1 ? "s" : "");
    for (int i = 0; i < n && i < NAMED && used < cap; ++i) {
        const int lanes = app_point_lanes(a, points[i]);
        used += (size_t)snprintf(text + used, cap - used, "%s %d (%d+%d)", i == 0 ? "" : ",", points[i], (lanes >> 4) & 0xf, lanes & 0xf);
    }
    if (used < cap) snprintf(text + used, cap - used, "%s %s ignored: a 1+2 or 2+1 stretch takes the lanes' width", n > NAMED ? " and more" : "", n > 1 ? "are" : "is");
    return 1;
}

/* ---- the fence step: the fence's height and picture (the road's) and each side's barrier Custom ---- */

static unsigned fence_bit_ok(unsigned bit)
{
    return bit == TRACE_KEY_FENCE_HEIGHT || bit == TRACE_KEY_FENCE_ART || bit == TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_LEFT) ||
           bit == TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT);
}

static int fence_value_of(const TraceKey *k, unsigned bit)
{
    return bit == TRACE_KEY_FENCE_HEIGHT ? k->fence_tall : bit == TRACE_KEY_FENCE_ART ? k->fence_art : k->barrier_custom[bit == TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT)];
}

void app_set_point_fence(App *a, int point, unsigned bit, int value)
{
    if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count || !fence_bit_ok(bit)) return;
    const int at = track_trace_key_find(a->trace.keys, a->trace.key_count, point);
    if (at >= 0 && (a->trace.keys[at].has & bit) && fence_value_of(&a->trace.keys[at], bit) == value) return;   /* the same: nothing changed */
    if (!track_trace_key_set_fence(&a->trace.keys, &a->trace.key_count, &a->trace.key_cap, point, bit, value)) return;   /* a value out of range */
    a->dirty = 1;
}

void app_clear_point_fence(App *a, int point, unsigned bit)
{
    if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count || !fence_bit_ok(bit)) return;
    const int at = track_trace_key_find(a->trace.keys, a->trace.key_count, point);
    if (at < 0 || !(a->trace.keys[at].has & bit)) return;   /* nothing to clear */
    track_trace_key_clear_fence(a->trace.keys, &a->trace.key_count, point, bit);
    a->dirty = 1;
}

int app_point_fence(const App *a, int point, TraceFencePoint *out)
{
    if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count) return 0;
    TraceFencePoint *all = malloc(sizeof *all * (size_t)a->trace.count);
    if (all == NULL) return 0;
    track_trace_point_fence(a->trace.keys, a->trace.key_count, a->trace.count, all);
    *out = all[point];
    free(all);
    return 1;
}

int app_barrier_custom_note(const App *a, char *text, size_t cap)
{
    if (a->kind != APP_KIND_TRACK || cap == 0) return 0;
    static const char *const SIDE[2] = { "left", "right" };
    size_t used = 0;
    int n = 0;
    text[0] = '\0';
    for (int k = 0; k < a->trace.key_count; ++k)
        for (int s = 0; s < 2; ++s) {
            const TraceKey *key = &a->trace.keys[k];
            if (!(key->has & TRACE_KEY_BARRIER_CUSTOM(s)) || !key->barrier_custom[s]) continue;
            const int w = snprintf(text + used, cap - used, "%spoint %d (%s)", n == 0 ? "the barrier is set apart from the fence (Custom) from " : ", ",
                                   key->point, SIDE[s]);
            if (w > 0) used += (size_t)w < cap - used ? (size_t)w : cap - used - 1;
            ++n;
        }
    if (n > 0 && used + 1 < cap) snprintf(text + used, cap - used, ": the car stops where Barrier from edge says, not at the fence");
    return n > 0;
}
