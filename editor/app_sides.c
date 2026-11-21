/* The road's sides and lanes on the editor's road keys (steps 3 and 4; app.h): set and clear a side value or the lanes of a control point, as the Road
 * profile block edits them. */
#include "app.h"
#include "key_ops.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- dep87 part 2: an edit changes only what is selected ---- */

/* whether two key lists say the same */
static int same_keys(const TraceKey *a, int na, const TraceKey *b, int nb)
{
    if (na != nb) return 0;
    for (int i = 0; i < na; ++i)
        if (a[i].point != b[i].point || !key_same(&a[i], &b[i], ~0u)) return 0;
    return 1;
}

int app_set_scoped(App *a, int point, const TraceKey *value)
{
    if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count) return 0;
    int *points = malloc(sizeof *points * (size_t)a->trace.count);
    if (points == NULL) return snprintf(a->status, sizeof a->status, "out of memory"), 0;
    int count = 0;
    if (sel_count(&a->sel) > 1 && sel_has(&a->sel, point)) {
        for (int i = 0; i < a->trace.count; ++i)
            if (sel_has(&a->sel, i)) points[count++] = i;
    } else {
        points[count++] = point;
    }
    TraceKey *keys = NULL;
    int key_count = 0;
    const int ok = keys_set_scoped(&a->trace, points, count, value, &keys, &key_count);
    free(points);
    if (!ok) return snprintf(a->status, sizeof a->status, "out of memory"), 0;
    if (same_keys(a->trace.keys, a->trace.key_count, keys, key_count)) {
        free(keys);
        return 1;   /* the same: nothing changed */
    }
    free(a->trace.keys);
    a->trace.keys = keys, a->trace.key_count = key_count, a->trace.key_cap = key_count;
    a->dirty = 1;
    return 1;
}

void app_extend_point(App *a, int point, unsigned mask, int to_end)
{
    if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count) return;
    int last = point;
    if (sel_count(&a->sel) > 1 && sel_has(&a->sel, point))   /* the end of the run it is in */
        while (last + 1 < a->trace.count && sel_has(&a->sel, last + 1)) ++last;
    TraceKey *keys = NULL;
    int key_count = 0;
    if (!keys_extend(&a->trace, last, mask, to_end, &keys, &key_count)) {
        snprintf(a->status, sizeof a->status, "out of memory");
        return;
    }
    if (same_keys(a->trace.keys, a->trace.key_count, keys, key_count)) {
        free(keys);
        return;
    }
    free(a->trace.keys);
    a->trace.keys = keys, a->trace.key_count = key_count, a->trace.key_cap = key_count;
    a->dirty = 1;
}

int app_copy_profile(App *a)
{
    if (a->kind != APP_KIND_TRACK || a->selected < 0 || a->selected >= a->trace.count) return snprintf(a->status, sizeof a->status, "select a point to copy its profile"), 0;
    KeyState st;
    if (!key_state_make(&a->trace, &st)) return snprintf(a->status, sizeof a->status, "out of memory"), 0;
    a->profile_clip = key_state_at(&st, a->selected, KEY_RAMPED | KEY_HELD);
    key_state_free(&st);
    snprintf(a->status, sizeof a->status, "profile of point %d copied", a->selected);
    return 1;
}

int app_paste_profile(App *a)
{
    if (a->kind != APP_KIND_TRACK) return 0;
    if (a->profile_clip.has == 0) return snprintf(a->status, sizeof a->status, "no profile copied: select a point and Copy profile first"), 0;
    if (a->selected < 0 || a->selected >= a->trace.count) return snprintf(a->status, sizeof a->status, "select the points to paste the profile on"), 0;
    const int point = a->selected;   /* app_set_scoped takes the whole selection when it is one of several */
    if (!app_set_scoped(a, point, &a->profile_clip)) return 0;
    app_track_commit(a);   /* one step to undo */
    if (sel_count(&a->sel) > 1 && sel_has(&a->sel, point)) {
        char which[96];
        sel_describe(&a->sel, which, sizeof which);
        snprintf(a->status, sizeof a->status, "profile pasted on points %s", which);
    } else {
        snprintf(a->status, sizeof a->status, "profile pasted on point %d", point);
    }
    return 1;
}

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
    TraceKey v;
    memset(&v, 0, sizeof v);
    v.has = TRACE_KEY_SIDE(field, side), v.side[field][side] = side_value(field, value);
    app_set_scoped(a, point, &v);
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
    TraceKey v;
    memset(&v, 0, sizeof v);
    v.has = TRACE_KEY_LANES, v.lanes = lanes;
    app_set_scoped(a, point, &v);
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

void app_set_point_fence(App *a, int point, unsigned bit, int value)
{
    if (a->kind != APP_KIND_TRACK || point < 0 || point >= a->trace.count || !fence_bit_ok(bit)) return;
    if (value < 0 || value >= (bit == TRACE_KEY_FENCE_ART ? TRACE_FENCE_PICTURES : 2)) return;   /* a value out of range */
    TraceKey v;
    memset(&v, 0, sizeof v);
    v.has = bit;
    if (bit == TRACE_KEY_FENCE_HEIGHT) v.fence_tall = value;
    else if (bit == TRACE_KEY_FENCE_ART) v.fence_art = value;
    else v.barrier_custom[bit == TRACE_KEY_BARRIER_CUSTOM(TRACK_GEN_RIGHT)] = value;
    app_set_scoped(a, point, &v);
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
