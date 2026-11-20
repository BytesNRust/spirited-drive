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
        { 2, TRACE_SIDE_BARRIER, L, 21, "barrier" }, { 2, TRACE_SIDE_WALL, R, 2, "barrier" }, { 3, TRACE_SIDE_FENCE, L, 0.5, "fence" },
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

/* `count` level nodes on a straight, 6 units apart, the template's width and sides */
static void straight_nodes(TrackGenNode *n, int count)
{
    memset(n, 0, sizeof *n * (size_t)count);
    for (int i = 0; i < count; ++i) n[i].position[2] = 6 * 65536 * i, n[i].width = WIDTH;
}

static void set_side(TrackGenNode *n, int side, unsigned bit, double eighths)
{
    n->side[side].has |= bit;
    if (bit == TRACK_GEN_SIDE_SHOULDER) n->side[side].shoulder = eighths;
    else n->side[side].barrier = eighths;
}

/* the limits of the sides: a shoulder or a wall that moves more than TRACE_SIDE_STEP_WARN_M from one node to the next is amber (a side no node
 * speaks of is the template's 3 m), and a wall past the 255 eighths a node holds from the centre is red, on any node, the first too */
static void test_side_flags(void)
{
    enum { N = 6 };
    CHECK_EQ_INT(TRACE_FLAG_SIDE_WARN, 32);
    CHECK_EQ_INT(TRACE_FLAG_WALL_FAR, 64);
    CHECK((TRACE_FLAG_RED_BITS & TRACE_FLAG_WALL_FAR) != 0 && (TRACE_FLAG_WARN_BITS & TRACE_FLAG_SIDE_WARN) != 0);
    CHECK((TRACE_FLAG_RED_BITS & TRACE_FLAG_WARN_BITS) == 0);
    CHECK_EQ_INT(sizeof(TraceFlags), 2);
    TrackGenNode n[N];
    TraceFlags flags[N];
    straight_nodes(n, N);
    track_trace_flags(n, N, flags);
    for (int i = 0; i < N; ++i) CHECK_EQ_INT(flags[i], 0);

    set_side(&n[2], R, TRACK_GEN_SIDE_SHOULDER, 24 + 10);   /* 1.25 m more than the template's, and back: on the limit, nothing */
    track_trace_flags(n, N, flags);
    for (int i = 0; i < N; ++i) CHECK_EQ_INT(flags[i], 0);
    set_side(&n[2], R, TRACK_GEN_SIDE_SHOULDER, 24 + 11);   /* 1.375 m: node 2 and node 3 (back to 3 m) */
    set_side(&n[5], L, TRACK_GEN_SIDE_BARRIER, 24 - 16);    /* the wall 2 m nearer at the last node */
    track_trace_flags(n, N, flags);
    CHECK_EQ_INT(flags[2], TRACE_FLAG_SIDE_WARN);
    CHECK_EQ_INT(flags[3], TRACE_FLAG_SIDE_WARN);
    CHECK_EQ_INT(flags[4], 0);
    CHECK_EQ_INT(flags[5], TRACE_FLAG_SIDE_WARN);

    straight_nodes(n, N);
    n[0].width = n[1].width = 95;   /* a wall 20 m out: 95 + 160 = 255, the most a node holds */
    set_side(&n[0], L, TRACK_GEN_SIDE_BARRIER, 160);
    set_side(&n[1], L, TRACK_GEN_SIDE_BARRIER, 160);
    track_trace_flags(n, N, flags);
    CHECK_EQ_INT(flags[0], 0);
    CHECK_EQ_INT(flags[1] & TRACE_FLAG_WALL_FAR, 0);
    n[0].width = n[1].width = 96;
    track_trace_flags(n, N, flags);
    CHECK_EQ_INT(flags[0], TRACE_FLAG_WALL_FAR);   /* the start, too: it has no node before it, but its wall is its own */
    CHECK_EQ_INT(flags[1], TRACE_FLAG_WALL_FAR);
    CHECK((flags[2] & TRACE_FLAG_WALL_FAR) == 0 && (flags[2] & TRACE_FLAG_SIDE_WARN) != 0);   /* back to 3 m from 20 m */
    {   /* a wall the node does not give is the template's, which the generator writes itself: never red, however wide the road */
        TrackGenNode wide[2];
        TraceFlags f[2];
        straight_nodes(wide, 2);
        wide[0].width = wide[1].width = 250;
        track_trace_flags(wide, 2, f);
        CHECK_EQ_INT(f[0] | f[1], 0);
    }

    TraceProblem rows[N];
    const int total = track_trace_problems(n, N, flags, rows, N);
    CHECK_EQ_INT(total, 3);   /* red 0 and 1, then amber 2 (side and width) */
    CHECK(rows[0].node == 0 && rows[0].red && rows[1].node == 1 && rows[1].red && rows[2].node == 2 && !rows[2].red);
    CHECK(fabs(rows[0].wall_m - 32.0) < 1e-9);
    CHECK(fabs(rows[2].side_step - 17.0) < 1e-9);   /* 20 m to 3 m */
    char text[TRACE_PROBLEM_TEXT];
    track_trace_problem_text(&rows[0], text, sizeof text);
    CHECK(strcmp(text, "node 0: a barrier 32.0 m from the centre (more than 31.875 m), can't be built") == 0);
    if (strcmp(text, "node 0: a barrier 32.0 m from the centre (more than 31.875 m), can't be built") != 0) fprintf(stderr, "%s\n", text);
    track_trace_problem_text(&rows[2], text, sizeof text);
    CHECK(strstr(text, "a shoulder or barrier moves 17.00 m (more than 1.25 m), may not look right") != NULL);
    if (strstr(text, "moves 17.00 m") == NULL) fprintf(stderr, "%s\n", text);

    TraceSummaryLine lines[TRACE_SUMMARY_LINES];
    const int used = track_trace_summary(flags, N, lines);
    CHECK(used >= 2);
    CHECK(lines[0].red && lines[0].nodes == 2);
    CHECK(strcmp(lines[0].text, "2 nodes have a barrier more than 31.875 m from the centre: the track can't be built") == 0);
    if (used >= 2) {
        CHECK(!lines[used - 1].red && lines[used - 1].nodes == 1);
        CHECK(strcmp(lines[used - 1].text, "1 node moves a shoulder or a barrier by more than 1.25 m: it may not look right in the game") == 0);
        if (strcmp(lines[used - 1].text, "1 node moves a shoulder or a barrier by more than 1.25 m: it may not look right in the game") != 0)
            fprintf(stderr, "%s\n", lines[used - 1].text);
    }
}

/* every rule at once fits the row's text and the summary's lines */
static void test_all_rules_fit(void)
{
    TraceProblem p;
    memset(&p, 0, sizeof p);
    p.node = 12345;
    p.red = 1;
    p.flags = TRACE_FLAG_TURN | TRACE_FLAG_RADIUS | TRACE_FLAG_WIDTH_WARN | TRACE_FLAG_SIDE_WARN | TRACE_FLAG_WALL_FAR;
    p.turn = 123.4, p.radius = 12.3, p.width_step = 12.25, p.side_step = 18.75, p.wall_m = 34.875;
    char text[TRACE_PROBLEM_TEXT];
    track_trace_problem_text(&p, text, sizeof text);
    CHECK(strlen(text) + 1 < sizeof text);
    CHECK(strstr(text, ", can't be built") != NULL);
    TraceFlags all = TRACE_FLAG_RED_BITS | TRACE_FLAG_WARN_BITS;
    TraceSummaryLine lines[TRACE_SUMMARY_LINES];
    CHECK_EQ_INT(track_trace_summary(&all, 1, lines), TRACE_SUMMARY_LINES);   /* a line per rule */
    for (int i = 0; i < TRACE_SUMMARY_LINES; ++i) CHECK(strlen(lines[i].text) + 1 < TRACE_SUMMARY_TEXT);
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
    test_side_flags();
    test_all_rules_fit();
    return TEST_REPORT();
}
