/* The road profile keys of a run of control points (key_ops.h). */
#include "key_ops.h"

#include "formats/track_gen.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SIDE_BITS(field) (TRACE_KEY_SIDE(field, 0) | TRACE_KEY_SIDE(field, 1))
#define KEY_KNOWN (KEY_RAMPED | KEY_HELD)

/* each side's bit with its partner's: a mirror or a reverse moves one side's values to the other */
static unsigned both_sides(unsigned has)
{
    for (int f = 0; f < TRACE_SIDE_FIELDS; ++f)
        if (has & SIDE_BITS(f)) has |= SIDE_BITS(f);
    return has;
}

int key_state_make(const Trace *t, KeyState *s)
{
    memset(s, 0, sizeof *s);
    const int n = t->count;
    s->count = n;
    for (int k = 0; k < t->key_count; ++k) s->used |= t->keys[k].has;
    s->used = both_sides(s->used) & KEY_KNOWN;
    const size_t c = (size_t)(n > 0 ? n : 1);
    s->width_m = malloc(sizeof *s->width_m * c), s->bank_deg = malloc(sizeof *s->bank_deg * c), s->side = malloc(sizeof *s->side * c);
    s->surface[0] = malloc(sizeof *s->surface[0] * c), s->surface[1] = malloc(sizeof *s->surface[1] * c), s->lanes = malloc(sizeof *s->lanes * c);
    if (!s->width_m || !s->bank_deg || !s->side || !s->surface[0] || !s->surface[1] || !s->lanes) return key_state_free(s), 0;
    track_trace_point_widths(t->points, n, t->width, t->keys, t->key_count, s->width_m);
    track_trace_point_banks(t->points, n, t->keys, t->key_count, s->bank_deg);
    track_trace_point_sides(t->points, n, t->keys, t->key_count, s->side);
    track_trace_point_surfaces(t->keys, t->key_count, n, s->surface[0], s->surface[1]);
    track_trace_point_lanes(t->keys, t->key_count, n, s->lanes);
    return 1;
}

void key_state_free(KeyState *s)
{
    free(s->width_m), free(s->bank_deg), free(s->side), free(s->surface[0]), free(s->surface[1]), free(s->lanes);
    memset(s, 0, sizeof *s);
}

TraceKey key_state_at(const KeyState *s, int i, unsigned mask)
{
    TraceKey k;
    memset(&k, 0, sizeof k);
    k.point = i;
    if (i < 0 || i >= s->count) return k;
    if (s->lanes[i] != TRACK_GEN_LANES_1_1) mask &= ~(unsigned)TRACE_KEY_WIDTH;   /* a lane stretch ignores a width key */
    k.has = mask & KEY_KNOWN;
    k.width_m = s->width_m[i], k.bank_deg = s->bank_deg[i], k.lanes = s->lanes[i];
    k.shoulder_l_surface = s->surface[0][i], k.shoulder_r_surface = s->surface[1][i];
    memcpy(k.side, s->side[i], sizeof k.side);
    return k;
}

TraceKey key_swapped(TraceKey k)
{
    TraceKey w = k;
    w.has = k.has & ~(unsigned)(SIDE_BITS(0) | SIDE_BITS(1) | SIDE_BITS(2) | SIDE_BITS(3));
    for (int f = 0; f < TRACE_SIDE_FIELDS; ++f) {
        if (k.has & TRACE_KEY_SIDE(f, 0)) w.has |= TRACE_KEY_SIDE(f, 1);
        if (k.has & TRACE_KEY_SIDE(f, 1)) w.has |= TRACE_KEY_SIDE(f, 0);
        w.side[f][0] = k.side[f][1], w.side[f][1] = k.side[f][0];
    }
    w.shoulder_l_surface = k.shoulder_r_surface, w.shoulder_r_surface = k.shoulder_l_surface;
    w.bank_deg = -k.bank_deg;
    w.lanes = k.lanes == TRACK_GEN_LANES_1_2 ? TRACK_GEN_LANES_2_1 : k.lanes == TRACK_GEN_LANES_2_1 ? TRACK_GEN_LANES_1_2 : k.lanes;
    return w;
}

static int same_double(double a, double b) { return fabs(a - b) < 1e-9; }

int key_same(const TraceKey *a, const TraceKey *b, unsigned mask)
{
    if ((a->has & mask) != (b->has & mask)) return 0;
    const unsigned m = a->has & mask;
    if ((m & TRACE_KEY_WIDTH) && !same_double(a->width_m, b->width_m)) return 0;
    if ((m & TRACE_KEY_BANK) && !same_double(a->bank_deg, b->bank_deg)) return 0;
    if ((m & TRACE_KEY_LANES) && a->lanes != b->lanes) return 0;
    if ((m & TRACE_KEY_SHOULDER_SURFACE) && (a->shoulder_l_surface != b->shoulder_l_surface || a->shoulder_r_surface != b->shoulder_r_surface)) return 0;
    for (int f = 0; f < TRACE_SIDE_FIELDS; ++f)
        for (int side = 0; side < 2; ++side)
            if ((m & TRACE_KEY_SIDE(f, side)) && !same_double(a->side[f][side], b->side[f][side])) return 0;
    return 1;
}

/* dst takes src's values of `mask` (over its own); a surface side src leaves to inherit is not taken */
static void put(TraceKey *dst, const TraceKey *src, unsigned mask)
{
    const unsigned m = src->has & mask;
    if (m & TRACE_KEY_WIDTH) dst->width_m = src->width_m;
    if (m & TRACE_KEY_BANK) dst->bank_deg = src->bank_deg;
    if (m & TRACE_KEY_LANES) dst->lanes = src->lanes;
    if (m & TRACE_KEY_SHOULDER_SURFACE) {
        if (src->shoulder_l_surface != TRACE_SURFACE_INHERIT) dst->shoulder_l_surface = src->shoulder_l_surface;
        if (src->shoulder_r_surface != TRACE_SURFACE_INHERIT) dst->shoulder_r_surface = src->shoulder_r_surface;
    }
    for (int f = 0; f < TRACE_SIDE_FIELDS; ++f)
        for (int side = 0; side < 2; ++side)
            if (m & TRACE_KEY_SIDE(f, side)) dst->side[f][side] = src->side[f][side];
    dst->has |= m;
}

static TraceKey *grid_new(int n)
{
    TraceKey *g = calloc((size_t)(n > 0 ? n : 1), sizeof *g);
    for (int i = 0; g != NULL && i < n; ++i) g[i].point = i;
    return g;
}

/* the grid's keys that set something, as a list (the grid is freed) */
static int grid_list(TraceKey *g, int n, TraceKey **out, int *count)
{
    int c = 0;
    for (int i = 0; i < n; ++i)
        if (g[i].has != 0) g[c++] = g[i];
    *count = c;
    if (c == 0) {
        free(g), *out = NULL;
        return 1;
    }
    TraceKey *list = realloc(g, sizeof *list * (size_t)c);
    *out = list != NULL ? list : g;
    return 1;
}

/* the points just outside s..e keep their ramped values, the tail's first segment its held ones (key_ops.h) */
static void pin_outside(TraceKey *g, int n, const KeyState *st, int s, int e, int was_before, int was_after, unsigned used)
{
    if (s > 0) {
        const TraceKey k = key_state_at(st, was_before, used & KEY_RAMPED);
        put(&g[s - 1], &k, KEY_RAMPED);
    }
    if (e < n - 1) {
        const TraceKey held = key_state_at(st, was_after - 1, used & KEY_HELD), ramped = key_state_at(st, was_after, used & KEY_RAMPED);
        put(&g[e], &held, KEY_HELD);
        put(&g[e + 1], &ramped, KEY_RAMPED);
    }
}

int keys_clip(const Trace *t, int s, int e, TraceKey **out, int *count)
{
    KeyState st;
    if (s < 0 || e >= t->count || s >= e || !key_state_make(t, &st)) return 0;
    const int m = e - s + 1;
    TraceKey *g = grid_new(m);
    if (g == NULL) return key_state_free(&st), 0;
    g[0] = key_state_at(&st, s, KEY_KNOWN);   /* every value as it is at the first point: Paste keeps the ones its road needs */
    g[0].point = 0;
    for (int k = 0; k < t->key_count; ++k)
        if (t->keys[k].point > s && t->keys[k].point < e) put(&g[t->keys[k].point - s], &t->keys[k], KEY_KNOWN);
    const TraceKey last = key_state_at(&st, e, st.used & KEY_RAMPED);
    put(&g[m - 1], &last, KEY_RAMPED);
    key_state_free(&st);
    return grid_list(g, m, out, count);
}

int keys_paste(const Trace *t, int anchor, const TraceKey *clip, int clip_count, int m, TraceKey **out, int *count)
{
    KeyState st;
    const int n = t->count, n2 = n + m - 1;
    if (anchor < 0 || anchor >= n || m < 2 || !key_state_make(t, &st)) return 0;
    TraceKey *g = grid_new(n2);
    if (g == NULL) return key_state_free(&st), 0;
    for (int k = 0; k < t->key_count; ++k) {
        const int p = t->keys[k].point;
        if (p >= 0 && p < n) put(&g[p <= anchor ? p : p + m - 1], &t->keys[k], KEY_KNOWN);
    }
    unsigned applied = 0;
    for (int c = 0; c < clip_count; ++c) {
        const TraceKey *ck = &clip[c];
        if (ck->point < 0 || ck->point >= m) continue;
        unsigned mask = ck->has & KEY_KNOWN;
        if (ck->point == 0) {   /* the first point's values: those this road uses, or that differ from what it has there */
            const TraceKey here = key_state_at(&st, anchor, KEY_KNOWN);
            for (unsigned bit = 1; bit != 0 && bit <= KEY_KNOWN; bit <<= 1)
                if ((mask & bit) && !(st.used & bit) && key_same(ck, &here, bit)) mask &= ~bit;
        }
        put(&g[anchor + ck->point], ck, mask);
        applied |= mask;
    }
    pin_outside(g, n2, &st, anchor, anchor + m - 1, anchor - 1, anchor + 1, both_sides(st.used | applied));
    key_state_free(&st);
    return grid_list(g, n2, out, count);
}

int keys_mirror(const Trace *t, int s, int e, TraceKey **out, int *count)
{
    KeyState st;
    const int n = t->count;
    if (s < 0 || e >= n || s >= e || !key_state_make(t, &st)) return 0;
    TraceKey *g = grid_new(n);
    if (g == NULL) return key_state_free(&st), 0;
    for (int k = 0; k < t->key_count; ++k)
        if (t->keys[k].point >= 0 && t->keys[k].point < n) put(&g[t->keys[k].point], &t->keys[k], KEY_KNOWN);
    const TraceKey first = key_state_at(&st, s, st.used), last = key_state_at(&st, e, st.used & KEY_RAMPED);
    put(&g[s], &first, KEY_KNOWN);
    put(&g[e], &last, KEY_RAMPED);
    for (int i = s; i <= e; ++i) {   /* the run's values seen from the other side; the last point's held ones are the tail's */
        const TraceKey was = g[i], sw = key_swapped(was);
        TraceKey k;
        memset(&k, 0, sizeof k);
        k.point = i;
        put(&k, &sw, i < e ? KEY_KNOWN : KEY_RAMPED);
        if (i == e) put(&k, &was, KEY_HELD);
        g[i] = k;
    }
    pin_outside(g, n, &st, s, e, s - 1, e + 1, st.used);
    key_state_free(&st);
    return grid_list(g, n, out, count);
}

int keys_reverse(const Trace *t, int s, int e, TraceKey **out, int *count)
{
    KeyState st;
    const int n = t->count;
    if (s < 0 || e >= n || s >= e || !key_state_make(t, &st)) return 0;
    TraceKey *g = grid_new(n), *ramp = grid_new(n);
    if (g == NULL || ramp == NULL) return free(g), free(ramp), key_state_free(&st), 0;
    for (int k = 0; k < t->key_count; ++k) {
        const int p = t->keys[k].point;
        if (p < 0 || p >= n) continue;
        put(p < s || p > e ? &g[p] : &ramp[p], &t->keys[k], p < s || p > e ? KEY_KNOWN : KEY_RAMPED);
    }
    const TraceKey first = key_state_at(&st, s, st.used & KEY_RAMPED), last = key_state_at(&st, e, st.used & KEY_RAMPED);
    put(&ramp[s], &first, KEY_RAMPED);
    put(&ramp[e], &last, KEY_RAMPED);
    for (int i = s; i <= e; ++i) {   /* a ramped value on the mirror-image point */
        const TraceKey sw = key_swapped(ramp[i]);
        put(&g[s + e - i], &sw, KEY_RAMPED);
    }
    TraceKey before;
    memset(&before, 0, sizeof before);
    for (int j = s; j < e; ++j) {   /* segment j takes old segment s+e-1-j's held values, from the other side; a key where they change */
        const TraceKey held = key_swapped(key_state_at(&st, s + e - 1 - j, st.used & KEY_HELD));
        if (j == s || !key_same(&held, &before, KEY_HELD)) put(&g[j], &held, KEY_HELD);
        before = held;
    }
    free(ramp);
    pin_outside(g, n, &st, s, e, s - 1, e + 1, st.used);
    key_state_free(&st);
    return grid_list(g, n, out, count);
}
