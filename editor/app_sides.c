/* The road's sides on the editor's road keys (step 3; app.h): set and clear a side value of a control point, as the Road key block edits them. */
#include "app.h"

#include <math.h>
#include <stdio.h>

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
