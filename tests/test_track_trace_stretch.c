/* Colour by (formats/track_trace_stretch.h): the classes of each mode on a hand-made road, and the runs of equal class. */
#include "formats/track_trace.h"
#include "formats/track_trace_stretch.h"
#include "formats/track_template.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { NODES = 160, STEP = 6, EDGE_SLACK = 5 };

/* 160 nodes 6 m apart: 0..39 straight, 40..79 turning right 1 degree a node (radius ~344 m), 80..119 straight, 120..159 left the same */
static TrackGenNode *road(void)
{
    TrackGenNode *n = calloc(NODES, sizeof *n);
    if (n == NULL) return NULL;
    const double unit = 16384.0 / 360.0;   /* heading units a degree */
    double h = 0;
    for (int k = 0; k < NODES; ++k) {
        if (k >= 40 && k < 80) h += unit;
        if (k >= 120) h -= unit;
        n[k].heading = h < 0 ? h + 16384.0 : h;
        n[k].position[2] = k * STEP * 65536;
        n[k].width = 40;
    }
    return n;
}

static int near_edge(int a, int b) { return abs(a - b) <= EDGE_SLACK; }

static void test_curve(void)
{
    TrackGenNode *n = road();
    uint8_t *c = malloc(NODES);
    CHECK(n != NULL && c != NULL);
    if (n == NULL || c == NULL) { free(n), free(c); return; }
    track_trace_node_classes(n, NODES, TRACE_COLOUR_CURVE, c);
    CHECK(c[20] == TRACE_CURVE_STRAIGHT && c[60] == TRACE_CURVE_RIGHT && c[100] == TRACE_CURVE_STRAIGHT && c[140] == TRACE_CURVE_LEFT);
    TraceStretch s[8];
    const int total = track_trace_stretches(c, NODES, s, 8);
    CHECK(total == 4);
    if (total == 4) {
        CHECK(near_edge(s[0].last, 39) && near_edge(s[1].last, 79) && near_edge(s[2].last, 119));
        CHECK(s[0].first == 0 && s[3].last == NODES - 1);
        CHECK(s[1].klass == TRACE_CURVE_RIGHT && s[3].klass == TRACE_CURVE_LEFT);
    }
    /* a wobble of 3 nodes the other way inside the right curve is not a stretch of its own */
    for (int k = 50; k < 53; ++k) n[k].heading -= 3 * 16384.0 / 360.0;
    track_trace_node_classes(n, NODES, TRACE_COLOUR_CURVE, c);
    CHECK(c[51] == TRACE_CURVE_RIGHT);
    /* a gentle bend (0.1 degree a node: radius ~3400 m) is a straight */
    for (int k = 0; k < NODES; ++k) n[k].heading = k * 16384.0 / 360.0 * 0.1;
    track_trace_node_classes(n, NODES, TRACE_COLOUR_CURVE, c);
    CHECK(track_trace_stretches(c, NODES, NULL, 0) == 1 && c[80] == TRACE_CURVE_STRAIGHT);
    free(n), free(c);
}

static void test_node_modes(void)
{
    TrackGenNode n[8];
    uint8_t c[8];
    memset(n, 0, sizeof n);
    n[1].bank = 0.5, n[2].bank = 5, n[3].bank = 15, n[4].bank = 25, n[5].bank = -5, n[6].bank = -25, n[7].bank = 35;
    track_trace_node_classes(n, 8, TRACE_COLOUR_BANK, c);
    CHECK(c[0] == 0 && c[1] == 0 && c[2] == 1 && c[3] == 2 && c[4] == 3 && c[5] == 4 && c[6] == 6 && c[7] == 3);
    memset(n, 0, sizeof n);   /* grade (percent): level under 2, then 2-8, 8-20 and 20 or more, up (1..3) and down (4..6) */
    n[1].grade = 1.5, n[2].grade = 5, n[3].grade = 12, n[4].grade = 30, n[5].grade = -5, n[6].grade = -30, n[7].grade = -1.9;
    track_trace_node_classes(n, 8, TRACE_COLOUR_GRADE, c);
    CHECK(c[0] == 0 && c[1] == 0 && c[2] == 1 && c[3] == 2 && c[4] == 3 && c[5] == 4 && c[6] == 6 && c[7] == 0);
    CHECK(track_trace_class_count(TRACE_COLOUR_GRADE) == TRACE_GRADE_CLASSES);
    CHECK(strcmp(track_trace_class_name(TRACE_COLOUR_GRADE, 0), "level") == 0);
    n[1].grade = 2, n[2].grade = 8, n[3].grade = 20, n[4].grade = -2, n[5].grade = -8, n[6].grade = -20;   /* the edges belong to the steeper class */
    track_trace_node_classes(n, 8, TRACE_COLOUR_GRADE, c);
    CHECK(c[1] == 1 && c[2] == 2 && c[3] == 3 && c[4] == 4 && c[5] == 5 && c[6] == 6);
    memset(n, 0, sizeof n);
    n[0].width = 24, n[1].width = 40, n[2].width = 48, n[3].width = 100, n[4].width = 40, n[4].width_r = 80;   /* half widths in 1/8 m: 6, 10, 12, 25 m; 15 m */
    track_trace_node_classes(n, 5, TRACE_COLOUR_WIDTH, c);
    CHECK(c[0] < c[1] && c[1] < c[2] && c[2] < c[3] && c[4] > c[2] && c[3] == TRACE_WIDTH_BANDS - 1);
    memset(n, 0, sizeof n);
    n[1].lanes = TRACK_GEN_LANES_1_1, n[2].lanes = TRACK_GEN_LANES_1_2, n[3].lanes = TRACK_GEN_LANES_2_1;
    track_trace_node_classes(n, 4, TRACE_COLOUR_LANES, c);
    CHECK(c[0] == 0 && c[1] == 0 && c[2] == 1 && c[3] == 2);
    memset(n, 0, sizeof n);
    n[1].side[TRACK_GEN_LEFT].has = TRACK_GEN_SIDE_TARMAC, n[1].side[TRACK_GEN_LEFT].tarmac = 1;
    n[2].side[TRACK_GEN_RIGHT].has = TRACK_GEN_SIDE_TARMAC, n[2].side[TRACK_GEN_RIGHT].tarmac = 1;
    n[3].side[TRACK_GEN_LEFT] = n[1].side[TRACK_GEN_LEFT], n[3].side[TRACK_GEN_RIGHT] = n[2].side[TRACK_GEN_RIGHT];
    n[4].side[TRACK_GEN_LEFT].has = TRACK_GEN_SIDE_TARMAC;   /* a surface key that says verge */
    track_trace_node_classes(n, 5, TRACE_COLOUR_SHOULDER, c);
    CHECK(c[0] == 0 && c[1] == 1 && c[2] == 2 && c[3] == 3 && c[4] == 0);
    memset(n, 0, sizeof n);
    n[1].fence.has = TRACK_GEN_FENCE_HEIGHT, n[1].fence.tall = 1;
    n[2].fence.has = TRACK_GEN_FENCE_SLOT, n[2].fence.slot = TRACE_FENCE_RAIL * TRACK_GENERIC_FENCE_PICTURE_STEP;
    n[3].fence.has = TRACK_GEN_FENCE_SLOT | TRACK_GEN_FENCE_HEIGHT, n[3].fence.slot = TRACE_FENCE_PARAPET * TRACK_GENERIC_FENCE_PICTURE_STEP, n[3].fence.tall = 1;
    track_trace_node_classes(n, 4, TRACE_COLOUR_FENCE, c);
    CHECK(c[0] == 0 && c[1] == 2 && c[2] == 3 && c[3] == 6);   /* 1 + 2 * picture + tall */
    track_trace_node_classes(n, 4, TRACE_COLOUR_LIMITS, c);
    CHECK(c[0] == 0 && c[1] == 0 && c[2] == 0 && c[3] == 0);
}

static void test_stretches(void)
{
    const uint8_t c[8] = { 1, 1, 0, 0, 0, 2, 2, 1 };
    TraceStretch s[2];
    CHECK(track_trace_stretches(c, 8, NULL, 0) == 4);
    CHECK(track_trace_stretches(c, 8, s, 2) == 4 && s[0].first == 0 && s[0].last == 1 && s[1].first == 2 && s[1].last == 4 && s[1].klass == 0);   /* more than cap: counted */
    int a = -1, b = -1;
    CHECK(track_trace_stretch_of(c, 8, 3, &a, &b) && a == 2 && b == 4);
    CHECK(track_trace_stretch_of(c, 8, 7, &a, &b) && a == 7 && b == 7);
    CHECK(!track_trace_stretch_of(c, 8, 8, &a, &b) && !track_trace_stretch_of(c, 8, -1, &a, &b));
    CHECK(track_trace_stretches(c, 0, s, 2) == 0 && track_trace_stretches(NULL, 0, NULL, 0) == 0);
}

static void test_node_at(void)
{
    CHECK(track_trace_node_at(0, 100, 11) == 0 && track_trace_node_at(100, 100, 11) == 10 && track_trace_node_at(50, 100, 11) == 5);
    CHECK(track_trace_node_at(54, 100, 11) == 5 && track_trace_node_at(56, 100, 11) == 6);   /* the nearest node */
    CHECK(track_trace_node_at(-5, 100, 11) == 0 && track_trace_node_at(150, 100, 11) == 10);   /* clamped */
    CHECK(track_trace_node_at(10, 100, 1) == 0 && track_trace_node_at(10, 100, 0) == 0 && track_trace_node_at(10, 0, 11) == 0);
}

static void test_names(void)
{
    for (int m = 0; m < TRACE_COLOUR_MODES; ++m) {
        const int n = track_trace_class_count((TraceColourBy)m);
        CHECK(n >= 1 && n <= 8);
        for (int k = 0; k < n; ++k) CHECK(m == TRACE_COLOUR_LIMITS || track_trace_class_name((TraceColourBy)m, k)[0] != '\0');
        CHECK(track_trace_class_name((TraceColourBy)m, n)[0] == '\0');
    }
}

int main(void)
{
    test_curve();
    test_node_modes();
    test_stretches();
    test_node_at();
    test_names();
    return TEST_REPORT();
}
