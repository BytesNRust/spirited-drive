/* Heights to the game's nodes (dep95 step 2): a control point's height is the profile's height there; node k's y is the monotone curve's height at the node's place on the
 * spline, in 16.16; its grade is the rise over the run to the next node as the integer positions have them, so the TRI is self-consistent; a flat road makes the same nodes
 * as one that never had heights. The limits on a grade are step 3's. */
#include "formats/track_gen.h"
#include "formats/track_template.h"
#include "formats/track_trace.h"
#include "formats/tri.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { POINTS = 5, WIDTH = 40 };   /* a straight of 5 points 100 m apart: 400 m */
#define ONE 65536.0

static void straight(TracePoint *p, const double *heights)
{
    for (int i = 0; i < POINTS; ++i) p[i] = (TracePoint){ .x = 0, .z = 100.0 * i, .h = heights != NULL ? heights[i] : 0 };
}

static TrackGenNode *nodes_of(const TracePoint *p, int *count)
{
    TrackGenNode *nodes = NULL;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    if (!track_trace_nodes(p, POINTS, WIDTH, &nodes, count, &err)) fprintf(stderr, "%s\n", err.message);
    return nodes;
}

static void test_flat(void)
{
    TracePoint p[POINTS] = { { .x = 0 } };
    straight(p, NULL);
    int count = 0;
    TrackGenNode *nodes = nodes_of(p, &count);
    CHECK(nodes != NULL && count > 8);
    for (int k = 0; nodes != NULL && k < count; ++k) CHECK(nodes[k].grade == 0.0 && nodes[k].position[1] == 0);
    free(nodes);
}

static void test_hill(void)
{
    const double heights[POINTS] = { 0, 4, 12, 8, 8 };   // up 4 m, up 8 m, down 4 m, flat: no slope past 12 percent
    TracePoint p[POINTS] = { { .x = 0 } };
    straight(p, heights);
    int count = 0;
    TrackGenNode *nodes = nodes_of(p, &count);
    CHECK(nodes != NULL && count > 8);
    if (nodes == NULL) return;
    // node y is the curve's height at the node's place: on a straight road the spline's parameter is z / 100 in the middle spans (the end spans' tangents are the ends')
    int checked = 0;
    for (int k = 0; k < count; ++k) {
        const double z = nodes[k].position[2] / ONE, u = z / 100.0;
        if (z < 100.0 || z > 300.0) continue;
        ++checked;
        int seg = (int)floor(u);
        if (seg > POINTS - 2) seg = POINTS - 2;
        const double want = track_trace_height_at(p, POINTS, seg, u - seg);
        CHECK(fabs(nodes[k].position[1] / ONE - want) < 2.0 / ONE);   // the rounding of the node's position, and of its z
    }
    CHECK(checked > 8);
    // the ends are the points' heights
    CHECK(nodes[0].position[1] == 0 && llabs((long long)nodes[count - 1].position[1] - (long long)(8.0 * ONE)) <= 1);
    // a grade is the rise over the run to the next node, from the integer positions; the last node has the one before it
    for (int k = 0; k + 1 < count; ++k) {
        const double run = hypot((nodes[k + 1].position[0] - nodes[k].position[0]) / ONE, (nodes[k + 1].position[2] - nodes[k].position[2]) / ONE);
        const double rise = (nodes[k + 1].position[1] - nodes[k].position[1]) / ONE;
        CHECK(run > 0 && fabs(nodes[k].grade - 100.0 * rise / run) < 1e-9);
    }
    CHECK(nodes[count - 1].grade == nodes[count - 2].grade);
    // spot checks: climbing in the first span, level at the top of the hill's flat end, descending after the hill
    CHECK(nodes[2].grade > 1.0 && nodes[2].grade < 12.0);
    CHECK(nodes[count - 3].grade == 0.0);
    int descending = 0;
    for (int k = 0; k < count; ++k) descending += nodes[k].grade < -1.0;
    CHECK(descending > 4);
    free(nodes);
}

static void test_steep_grade_is_in_the_nodes(void)
{
    const double heights[POINTS] = { 0, 150, 0, 0, 0 };   // 150 m in 100: far past what the generator builds (it refuses by node; step 3 shows it in Problems)
    TracePoint p[POINTS] = { { .x = 0 } };
    straight(p, heights);
    int count = 0;
    TrackGenNode *nodes = nodes_of(p, &count);
    CHECK(nodes != NULL);
    int steepest = 0;
    for (int k = 0; nodes != NULL && k < count; ++k)
        if (fabs(nodes[k].grade) > fabs(nodes[steepest].grade)) steepest = k;
    CHECK(nodes != NULL && fabs(nodes[steepest].grade) > 100.0);   // the nodes say how steep it is: the limits are step 3's
    free(nodes);
}

/* the TRI of a hill: the nodes' y in the file are the profile's, and the pitch bytes are the grade's angle (14-bit, 0x4000 a full turn, negative above 0x2000) */
static void test_tri_of_a_hill(void)
{
    const double heights[POINTS] = { 0, 4, 12, 8, 8 };
    TracePoint p[POINTS];
    straight(p, heights);
    int count = 0;
    TrackGenNode *nodes = nodes_of(p, &count);
    CHECK(nodes != NULL);
    if (nodes == NULL) return;
    uint8_t *tmpl = NULL, *out = NULL;
    size_t tmpl_len = 0, out_len = 0;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    CHECK(track_template_generic(&tmpl, &tmpl_len, &err));
    CHECK(track_generate_nodes_turned(nodes, count, tmpl, tmpl_len, &out, &out_len, &err));
    Tri tri;
    CHECK_EQ_INT(tri_open(&tri, out, out_len), TRI_OK);
    int checked = 0;
    for (int k = 0; k + 1 < count; k += 3) {
        TriNode n;
        CHECK_EQ_INT(tri_node(&tri, (uint32_t)k, &n), TRI_OK);
        CHECK(n.position.y == nodes[k].position[1]);
        int slope = n.slope & 0x3fff;
        if (slope > 0x2000) slope -= 0x4000;
        const double degrees = slope * 360.0 / 0x4000, want = atan(nodes[k].grade / 100.0) * 180.0 / M_PI;
        CHECK(fabs(degrees - want) < 0.05);
        ++checked;
    }
    CHECK(checked > 8);
    free(out);
    free(tmpl);
    free(nodes);
}

int main(void)
{
    test_flat();
    test_hill();
    test_steep_grade_is_in_the_nodes();
    test_tri_of_a_hill();
    return TEST_REPORT();
}
