/* The road's sides as road keys say them (dep59 editor phase 2, step 3): per key and per side the shoulder's width, the wall's distance, its kind and the
 * fence. Widths go from key to key by a smoothstep over the arc length (from the template's 3 m at the start when the first key is further on), kinds and
 * fences hold until the next key; a side no key speaks of is the template's (the nodes' side bits clear), so a trace with no side keys builds as before. */
#include "formats/track_trace.h"
#include "formats/track_gen.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDTH = 40, L = TRACK_GEN_LEFT, R = TRACK_GEN_RIGHT };

static double smooth(double t) { return t * t * (3 - 2 * t); }

/* a straight of `n` points `gap` m apart */
static void straight_points(TracePoint *p, int n, double gap)
{
    for (int i = 0; i < n; ++i) p[i].x = 0, p[i].z = gap * i;
}

static TraceKey side_key(int point, int field, int side, double value)
{
    TraceKey k;
    memset(&k, 0, sizeof k);
    k.point = point;
    k.has = TRACE_KEY_SIDE(field, side);
    k.side[field][side] = value;
    return k;
}

static TrackGenNode *nodes_of(const TracePoint *p, int n, const TraceKey *keys, int key_count, int *count)
{
    TrackGenNode *nodes = NULL;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    if (!track_trace_nodes_keyed(p, n, WIDTH, keys, key_count, &nodes, count, &err)) fprintf(stderr, "%s\n", err.message);
    return nodes;
}

/* the bits are their own: past the width (1), the bank (dev2, 1 << 1) and the shoulder's surface (dev3, 1 << 2), one per value and side */
static void test_bits(void)
{
    unsigned all = 0;
    for (int f = 0; f < TRACE_SIDE_FIELDS; ++f)
        for (int s = 0; s < 2; ++s) {
            const unsigned bit = TRACE_KEY_SIDE(f, s);
            CHECK(bit >= 1u << 3 && (all & bit) == 0);
            all |= bit;
        }
    CHECK((all & TRACE_KEY_WIDTH) == 0);
}

/* no side keys: no node says anything of its sides (the template's, byte for byte), whatever the width keys do */
static void test_no_side_keys(void)
{
    TracePoint p[6];
    straight_points(p, 6, 60);
    const TraceKey width = { .point = 2, .has = TRACE_KEY_WIDTH, .width_m = 12.0 };
    int count = 0;
    TrackGenNode *n = nodes_of(p, 6, &width, 1, &count);
    CHECK(n != NULL);
    int none = 1;
    for (int k = 0; n != NULL && k < count; ++k)
        if (n[k].side[L].has != 0 || n[k].side[R].has != 0) none = 0;
    CHECK(none);
    free(n);
}

/* a width (the shoulder, the wall's distance): from the template's 3 m at the start to the key's value at its point by a smoothstep, then held; the other
 * side says nothing */
static void test_widths_interpolate(void)
{
    TracePoint p[6];
    straight_points(p, 6, 60);   /* 300 m: a node every 6.25 m */
    const TraceKey keys[2] = { side_key(2, TRACE_SIDE_SHOULDER, R, 6.0), side_key(4, TRACE_SIDE_BARRIER, R, 12.0) };
    int count = 0;
    TrackGenNode *n = nodes_of(p, 6, keys, 2, &count);
    CHECK_EQ_INT(count, 49);
    if (n == NULL) return;
    int exact = 1;
    for (int k = 0; k < count; ++k) {
        const double s = k * 6.25;
        const double shoulder = s >= 120 ? 48 : 24 + 24 * smooth(s / 120), barrier = s >= 240 ? 96 : 24 + 72 * smooth(s / 240);
        if (!(n[k].side[R].has & TRACK_GEN_SIDE_SHOULDER) || fabs(n[k].side[R].shoulder - shoulder) > 1e-6) exact = 0;
        if (!(n[k].side[R].has & TRACK_GEN_SIDE_BARRIER) || fabs(n[k].side[R].barrier - barrier) > 1e-6) exact = 0;
        if (n[k].side[R].has & (TRACK_GEN_SIDE_WALL | TRACK_GEN_SIDE_FENCE)) exact = 0;
        if (n[k].side[L].has != 0) exact = 0;
    }
    CHECK(exact);
    free(n);

    TraceKey two[2] = { side_key(0, TRACE_SIDE_SHOULDER, L, 1.0), side_key(3, TRACE_SIDE_SHOULDER, L, 5.0) };   /* a key on point 0 is the start */
    n = nodes_of(p, 6, two, 2, &count);
    CHECK(n != NULL && n[0].side[L].shoulder == 8 && n[count - 1].side[L].shoulder == 40);
    CHECK(n != NULL && fabs(n[12].side[L].shoulder - (8 + 32 * smooth(75.0 / 180))) < 1e-6);
    free(n);
}

/* a kind (the wall's, the fence): nothing before the first key (the template's), the key's from its point to the next */
static void test_kinds_hold(void)
{
    TracePoint p[6];
    straight_points(p, 6, 60);
    const TraceKey keys[3] = { side_key(1, TRACE_SIDE_WALL, L, TRACK_GEN_WALL_HARD), side_key(3, TRACE_SIDE_WALL, L, TRACK_GEN_WALL_SOFT),
                               side_key(2, TRACE_SIDE_FENCE, R, 0) };
    int count = 0;
    TrackGenNode *n = nodes_of(p, 6, keys, 3, &count);
    if (n == NULL) { CHECK(0); return; }
    int exact = 1;
    for (int k = 0; k < count; ++k) {
        const double s = k * 6.25;
        const TrackGenSide *l = &n[k].side[L], *r = &n[k].side[R];
        if (s < 60 && (l->has & TRACK_GEN_SIDE_WALL)) exact = 0;
        if (s >= 60 && (!(l->has & TRACK_GEN_SIDE_WALL) || l->wall != (s < 180 ? TRACK_GEN_WALL_HARD : TRACK_GEN_WALL_SOFT))) exact = 0;
        if (s < 120 && (r->has & TRACK_GEN_SIDE_FENCE)) exact = 0;
        if (s >= 120 && (!(r->has & TRACK_GEN_SIDE_FENCE) || r->fence != 0)) exact = 0;
    }
    CHECK(exact);
    free(n);
}

/* values off their range, or on no point, are refused naming the point and what is wrong */
static void test_side_faults(void)
{
    TracePoint p[4];
    straight_points(p, 4, 100);
    static const struct { int point, field, side; double value; const char *words; } BAD[] = {
        { 1, TRACE_SIDE_SHOULDER, R, 20.25, "shoulder" }, { 1, TRACE_SIDE_SHOULDER, L, -0.25, "shoulder" },
        { 2, TRACE_SIDE_BARRIER, L, 21, "wall" }, { 2, TRACE_SIDE_WALL, R, 2, "wall" }, { 3, TRACE_SIDE_FENCE, L, 0.5, "fence" },
        { 4, TRACE_SIDE_FENCE, L, 1, "point 4" },
    };
    for (size_t b = 0; b < sizeof BAD / sizeof BAD[0]; ++b) {
        const TraceKey k = side_key(BAD[b].point, BAD[b].field, BAD[b].side, BAD[b].value);
        TrackGenNode *n = NULL;
        int count = 0;
        ExchangeError err;
        memset(&err, 0, sizeof err);
        CHECK(!track_trace_nodes_keyed(p, 4, WIDTH, &k, 1, &n, &count, &err) && n == NULL);
        char point[16];
        snprintf(point, sizeof point, "point %d", BAD[b].point);
        if (strstr(err.message, point) == NULL || strstr(err.message, BAD[b].words) == NULL) fprintf(stderr, "fault %zu: %s\n", b, err.message);
        CHECK(strstr(err.message, point) != NULL && strstr(err.message, BAD[b].words) != NULL);
    }
}

/* each control point's values, as the Road key block shows them dimmed when inherited: the template's before any key */
static void test_point_values(void)
{
    TracePoint p[6];
    straight_points(p, 6, 60);
    const TraceKey keys[2] = { side_key(2, TRACE_SIDE_SHOULDER, R, 6.0), side_key(3, TRACE_SIDE_WALL, L, TRACK_GEN_WALL_HARD) };
    double v[6][TRACE_SIDE_FIELDS][2];
    track_trace_point_sides(p, 6, keys, 2, v);
    CHECK(v[0][TRACE_SIDE_SHOULDER][R] == 3.0 && v[2][TRACE_SIDE_SHOULDER][R] == 6.0 && v[5][TRACE_SIDE_SHOULDER][R] == 6.0);
    CHECK(fabs(v[1][TRACE_SIDE_SHOULDER][R] - (3 + 3 * smooth(0.5))) < 1e-6);
    CHECK(v[3][TRACE_SIDE_SHOULDER][L] == TRACE_SHOULDER_DEFAULT_M && v[3][TRACE_SIDE_BARRIER][L] == TRACE_BARRIER_DEFAULT_M);
    CHECK(v[2][TRACE_SIDE_WALL][L] == TRACK_GEN_WALL_SOFT && v[3][TRACE_SIDE_WALL][L] == TRACK_GEN_WALL_HARD && v[5][TRACE_SIDE_WALL][L] == TRACK_GEN_WALL_HARD);
    CHECK(v[0][TRACE_SIDE_FENCE][L] == 1 && v[0][TRACE_SIDE_FENCE][R] == 1);   /* the generic template: a fence each side */
}

/* the key list's edits: a side value makes or joins the point's key, clearing it drops only its bit, and the key goes when nothing is left */
static void test_side_edits(void)
{
    TraceKey *keys = NULL;
    int count = 0, cap = 0;
    CHECK(track_trace_key_set_side(&keys, &count, &cap, 3, TRACE_SIDE_BARRIER, L, 8.0));
    CHECK(track_trace_key_set_width(&keys, &count, &cap, 3, 12.0));
    CHECK(track_trace_key_set_side(&keys, &count, &cap, 1, TRACE_SIDE_FENCE, R, 0));
    CHECK_EQ_INT(count, 2);
    if (count != 2) { free(keys); return; }
    CHECK(keys[0].point == 1 && keys[0].has == TRACE_KEY_SIDE(TRACE_SIDE_FENCE, R) && keys[0].side[TRACE_SIDE_FENCE][R] == 0);
    CHECK(keys[1].point == 3 && keys[1].has == (TRACE_KEY_WIDTH | TRACE_KEY_SIDE(TRACE_SIDE_BARRIER, L)) && keys[1].side[TRACE_SIDE_BARRIER][L] == 8.0);
    CHECK(track_trace_key_set_side(&keys, &count, &cap, 3, TRACE_SIDE_BARRIER, L, 9.5) && keys[1].side[TRACE_SIDE_BARRIER][L] == 9.5);   /* updated */
    track_trace_key_clear_width(keys, &count, 3);   /* the width goes, the side stays: the key stays */
    CHECK(count == 2 && keys[1].has == TRACE_KEY_SIDE(TRACE_SIDE_BARRIER, L));
    track_trace_key_clear_side(keys, &count, 3, TRACE_SIDE_BARRIER, L);
    CHECK_EQ_INT(count, 1);
    track_trace_key_clear_side(keys, &count, 1, TRACE_SIDE_FENCE, L);   /* a bit it does not have: nothing */
    CHECK_EQ_INT(count, 1);
    track_trace_key_clear_side(keys, &count, 1, TRACE_SIDE_FENCE, R);
    CHECK_EQ_INT(count, 0);
    free(keys);
}

int main(void)
{
    test_bits();
    test_no_side_keys();
    test_widths_interpolate();
    test_kinds_hold();
    test_side_faults();
    test_point_values();
    test_side_edits();
    return TEST_REPORT();
}
