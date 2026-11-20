/* Lanes on the road keys (dep59 editor phase 2, step 4): a key's lanes (1+1, 1+2, 2+1) hold until the next key that says lanes; a segment takes the
 * lanes of the last control point at or before its start (as dev3's surfaces: the strips' pictures go by segment). In a 1+2 or 2+1 stretch the road's
 * halves are the lanes' (5 m each: 1+2 is 5 m left, 10 m right), whatever the width keys say there; on the way in and out each half moves at most
 * 5 eighths of a metre a node (AL1 193..200: 44, 48 .. 80), and the two-lane half is never under a lane. A width key inside a stretch is ignored, and
 * named. A trace with no lanes keys makes the same nodes as before. */
#include "formats/track_trace.h"
#include "formats/track_gen.h"
#include "formats/track_template.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { POINTS = 7, WIDTH = 40 };   /* a straight of 7 points 100 m apart: 600 m */

static void straight(TracePoint *p)
{
    for (int i = 0; i < POINTS; ++i) p[i].x = 0, p[i].z = 100.0 * i;
}

static TraceKey lanes_key(int point, int lanes)
{
    TraceKey k;
    memset(&k, 0, sizeof k);
    k.point = point;
    k.has = TRACE_KEY_LANES;
    k.lanes = lanes;
    return k;
}

static TrackGenNode *nodes_of(const TracePoint *p, const TraceKey *keys, int key_count, int *count)
{
    TrackGenNode *nodes = NULL;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    if (!track_trace_nodes_keyed(p, POINTS, WIDTH, keys, key_count, &nodes, count, &err)) fprintf(stderr, "%s\n", err.message);
    return nodes;
}

static double right_of(const TrackGenNode *n) { return n->width_r > 0 ? n->width_r : n->width; }

/* the first node of the segment that starts at or after `metres` along the road */
static int segment_start_after(const TrackGenNode *n, int count, double metres)
{
    double *m = malloc(sizeof *m * (size_t)count);
    if (m == NULL) return -1;
    track_trace_distances(n, count, m);
    int k = 0;
    while (k < count && !(k % 4 == 0 && m[k] >= metres - 1e-6)) ++k;
    free(m);
    return k;
}

static void test_no_lanes_same_nodes(void)
{
    TracePoint p[POINTS];
    straight(p);
    TraceKey width = { .point = 3, .has = TRACE_KEY_WIDTH, .width_m = 14.0 };
    int a = 0, b = 0;
    TrackGenNode *before = nodes_of(p, &width, 1, &a);
    TraceKey both[2] = { width, lanes_key(5, TRACK_GEN_LANES_1_1) };   /* a 1+1 key is the road of before */
    TrackGenNode *after = nodes_of(p, both, 2, &b);
    CHECK(before != NULL && after != NULL && a == b);
    if (before != NULL && after != NULL && a == b) CHECK(memcmp(before, after, sizeof *before * (size_t)a) == 0);
    free(before);
    free(after);
}

/* 1+2 from point 2 (200 m) to point 4 (400 m): the halves, the lanes, and each step */
static void test_stretch(int lanes)
{
    TracePoint p[POINTS];
    straight(p);
    const TraceKey keys[2] = { lanes_key(2, lanes), lanes_key(4, TRACK_GEN_LANES_1_1) };
    int count = 0;
    TrackGenNode *n = nodes_of(p, keys, 2, &count);
    CHECK(n != NULL);
    if (n == NULL) return;
    const int from = segment_start_after(n, count, 200), to = segment_start_after(n, count, 400);
    CHECK(from > 0 && from % 4 == 0 && to > from && to % 4 == 0);
    const int two_right = lanes == TRACK_GEN_LANES_1_2;
    for (int k = 0; k < count; ++k) {
        const int in = k >= from && k < to;
        CHECK_EQ_INT(n[k].lanes, in ? lanes : 0);
        const double two = two_right ? right_of(&n[k]) : n[k].width, one = two_right ? n[k].width : right_of(&n[k]);
        if (in) CHECK(two >= TRACK_GEN_LANE);
        if (in && k >= from + 7) CHECK(two == 2 * TRACK_GEN_LANE && one == TRACK_GEN_LANE);   /* the taper done: 10 m and 5 m */
        if (k > 0) {
            const double two_before = two_right ? right_of(&n[k - 1]) : n[k - 1].width, one_before = two_right ? n[k - 1].width : right_of(&n[k - 1]);
            CHECK(fabs(two - two_before) <= 5 + 1e-9 && fabs(one - one_before) <= 5 + 1e-9);
        }
        if (k < from || k >= to + 9) CHECK(n[k].width == WIDTH && right_of(&n[k]) == WIDTH);   /* far from the stretch: the road of before */
    }
    CHECK(two_right ? right_of(&n[from]) == 45 : n[from].width == 45);   /* AL1's taper: 5 a node from the 5 m half */
    /* and back from 10 m after it, from the second node: the stretch's last segment draws its two-lane picture up to node `to` (a segment's
     * picture reaches the next segment's first row), so the half is still two lanes there; narrowing on that quad bends its dashed line */
    CHECK(two_right ? right_of(&n[to]) == 80 : n[to].width == 80);
    CHECK(two_right ? right_of(&n[to + 1]) == 75 : n[to + 1].width == 75);
    TraceFlags *flags = calloc((size_t)count, sizeof *flags);
    if (flags != NULL) {
        track_trace_flags(n, count, flags);
        for (int k = 0; k < count; ++k) CHECK((flags[k] & (TRACE_FLAG_WIDTH_WARN | TRACE_FLAG_RED_BITS)) == 0);   /* the taper is under the width's amber */
        free(flags);
    }
    uint8_t *tmpl = NULL, *out = NULL;
    size_t tsize = 0, len = 0;
    ExchangeError err = {0};
    CHECK(track_template_generic(&tmpl, &tsize, &err));
    CHECK(tmpl != NULL && track_generate_nodes_turned(n, count, tmpl, tsize, &out, &len, &err));   /* the generator takes it */
    if (err.message[0] != '\0') fprintf(stderr, "%s\n", err.message);
    free(out);
    free(tmpl);
    free(n);
}

/* a narrower road (8 m) going into 1+2: the two-lane half jumps to a lane at once (1 m), then grows 5 a node; the other half grows to a lane */
static void test_from_narrow(void)
{
    TracePoint p[POINTS];
    straight(p);
    const TraceKey keys[3] = { { .point = 0, .has = TRACE_KEY_WIDTH, .width_m = 8.0 }, lanes_key(2, TRACK_GEN_LANES_1_2), lanes_key(4, TRACK_GEN_LANES_1_1) };
    int count = 0;
    TrackGenNode *n = nodes_of(p, keys, 3, &count);
    CHECK(n != NULL);
    if (n == NULL) return;
    const int from = segment_start_after(n, count, 200);
    CHECK(n[from - 1].width == 32 && right_of(&n[from - 1]) == 32);
    CHECK(right_of(&n[from]) == TRACK_GEN_LANE && n[from].width == 37);
    CHECK(right_of(&n[from + 1]) == 45 && n[from + 1].width == 40);
    free(n);
}

/* a width key inside a stretch is ignored, and named: the halves stay the lanes'; one outside is not */
static void test_width_inside_ignored(void)
{
    TracePoint p[POINTS];
    straight(p);
    const TraceKey keys[4] = { lanes_key(2, TRACK_GEN_LANES_2_1), { .point = 3, .has = TRACE_KEY_WIDTH, .width_m = 14.0 }, lanes_key(4, TRACK_GEN_LANES_1_1),
                               { .point = 5, .has = TRACE_KEY_WIDTH, .width_m = 12.0 } };
    int count = 0;
    TrackGenNode *n = nodes_of(p, keys, 4, &count);
    CHECK(n != NULL);
    if (n == NULL) return;
    const int mid = segment_start_after(n, count, 300);
    CHECK(n[mid].width == 80 && right_of(&n[mid]) == 40);   /* 2+1: 10 m left, 5 m right, not 14 m */
    int ignored[4] = { -1, -1, -1, -1 };
    CHECK_EQ_INT(track_trace_lane_ignored_widths(p, POINTS, keys, 4, ignored, 4), 1);
    CHECK_EQ_INT(ignored[0], 3);
    CHECK_EQ_INT(track_trace_lane_ignored_widths(p, POINTS, keys, 4, NULL, 0), 1);   /* the count alone */
    free(n);
}

/* the key edits: set adds or changes the lanes of a point's key, clear drops that bit (the key goes with nothing left); the lanes of each point as the
 * keys make them (1+1 before any key) */
static void test_key_edits_and_points(void)
{
    TraceKey *keys = NULL;
    int count = 0, cap = 0;
    CHECK(track_trace_key_set_lanes(&keys, &count, &cap, 2, TRACK_GEN_LANES_1_2));
    CHECK(!track_trace_key_set_lanes(&keys, &count, &cap, 3, 0x22));   /* not one of the three */
    CHECK(track_trace_key_set_lanes(&keys, &count, &cap, 4, TRACK_GEN_LANES_1_1));
    CHECK_EQ_INT(count, 2);
    int lanes[POINTS];
    track_trace_point_lanes(keys, count, POINTS, lanes);
    const int want[POINTS] = { 0x11, 0x11, 0x12, 0x12, 0x11, 0x11, 0x11 };
    for (int i = 0; i < POINTS; ++i) CHECK_EQ_INT(lanes[i], want[i]);
    track_trace_key_clear_lanes(keys, &count, 2);
    CHECK_EQ_INT(count, 1);
    CHECK_EQ_INT(keys[0].point, 4);
    free(keys);
}

int main(void)
{
    test_no_lanes_same_nodes();
    test_stretch(TRACK_GEN_LANES_1_2);
    test_stretch(TRACK_GEN_LANES_2_1);
    test_from_narrow();
    test_width_inside_ignored();
    test_key_edits_and_points();
    return TEST_REPORT();
}
