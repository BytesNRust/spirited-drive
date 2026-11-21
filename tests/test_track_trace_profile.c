/* The height profile's figures for the editor's strip (track_trace.h): the vertical range a profile is drawn in, the height a mouse row means, and a slope as a
 * percentage or in degrees. */
#include "formats/track_trace.h"
#include "test.h"

#include <math.h>
#include <string.h>

static int near(double a, double b) { return fabs(a - b) < 1e-6; }

static void test_range(void)
{
    TracePoint flat[3] = { { .x = 0 }, { .x = 100 }, { .x = 200 } };
    double lo = 99, hi = -99;
    track_trace_profile_range(flat, 3, &lo, &hi);
    CHECK(near(hi - lo, TRACE_PROFILE_MIN_SPAN_M));   // a flat road still has room to be drawn and dragged in
    CHECK(near((lo + hi) / 2, 0));

    TracePoint hill[3] = { { .x = 0, .h = 0 }, { .x = 100, .h = 40 }, { .x = 200, .h = 10 } };
    track_trace_profile_range(hill, 3, &lo, &hi);
    CHECK(lo < 0 && hi > 40);   // padded: the extremes are not on the border
    CHECK(near(lo, -(hi - 40)));   // by the same margin on both sides

    track_trace_profile_range(NULL, 0, &lo, &hi);   // no points: the same room as a flat road
    CHECK(near(hi - lo, TRACE_PROFILE_MIN_SPAN_M));
}

static void test_rows(void)
{
    // row `top` is the range's top, `bottom` its bottom
    CHECK(near(track_trace_profile_height_at_row(100, 100, 300, -10, 30), 30));
    CHECK(near(track_trace_profile_height_at_row(300, 100, 300, -10, 30), -10));
    CHECK(near(track_trace_profile_height_at_row(200, 100, 300, -10, 30), 10));
    CHECK(near(track_trace_profile_row_of_height(10, 100, 300, -10, 30), 200));
    CHECK(near(track_trace_profile_height_at_row(50, 100, 300, -10, 30), 40));   // above the box: not clamped here (the editor clamps)
    CHECK(near(track_trace_profile_height_at_row(1, 5, 5, 0, 10), 0));   // a box with no height: the bottom, not a division by zero
}

static void test_grade(void)
{
    char text[32];
    CHECK(near(track_trace_grade(5, 100, TRACE_GRADE_PERCENT), 5));
    CHECK(near(track_trace_grade(-12.5, 100, TRACE_GRADE_PERCENT), -12.5));
    CHECK(near(track_trace_grade(100, 100, TRACE_GRADE_DEGREES), 45));
    CHECK(near(track_trace_grade(0, 100, TRACE_GRADE_DEGREES), 0));
    CHECK(near(track_trace_grade(5, 0, TRACE_GRADE_PERCENT), 0));   // no run: no slope

    track_trace_grade_text(4.25, 100, TRACE_GRADE_PERCENT, text, sizeof text);
    CHECK(strcmp(text, "+4.2 %") == 0 || strcmp(text, "+4.3 %") == 0);
    track_trace_grade_text(-10, 100, TRACE_GRADE_PERCENT, text, sizeof text);
    CHECK(strcmp(text, "-10.0 %") == 0);
    track_trace_grade_text(0, 100, TRACE_GRADE_PERCENT, text, sizeof text);
    CHECK(strcmp(text, "0.0 %") == 0);   // level: no sign
    track_trace_grade_text(100, 100, TRACE_GRADE_DEGREES, text, sizeof text);
    CHECK(strcmp(text, "+45.0\xc2\xb0") == 0);
    track_trace_grade_text(3, 0, TRACE_GRADE_PERCENT, text, sizeof text);
    CHECK(strcmp(text, "-") == 0);
    track_trace_grade_text(1, 10, TRACE_GRADE_PERCENT, text, 4);   // a short buffer is cut, never overrun
    CHECK(strlen(text) < 4);
}

static void test_segment_grade(void)
{
    TracePoint p[3] = { { .x = 0, .z = 0, .h = 0 }, { .x = 100, .z = 0, .h = 10 }, { .x = 100, .z = 50, .h = 10 } };
    double metres[3];
    track_trace_point_distances(p, 3, metres);
    CHECK(near(track_trace_segment_grade(p, metres, 3, 0, TRACE_GRADE_PERCENT), 10 / metres[1] * 100));
    CHECK(near(track_trace_segment_grade(p, metres, 3, 1, TRACE_GRADE_PERCENT), 0));
    CHECK(near(track_trace_segment_grade(p, metres, 3, 2, TRACE_GRADE_PERCENT), 0));   // not a segment
    CHECK(near(track_trace_segment_grade(p, metres, 3, -1, TRACE_GRADE_PERCENT), 0));
}

/* the nodes where the slope's angle changes more than TRACE_GRADE_CHANGE_WARN_DEG from the node before, on the height curve at node spacing */
static void test_amber_nodes(void)
{
    uint8_t amber[400];
    double metres[4];
    /* a gentle hill over 100 m steps: the angle changes a little at a time: no amber */
    const TracePoint gentle[4] = { { 0, 0, 0 }, { 0, 100, 0 }, { 0, 200, 10 }, { 0, 300, 10 } };
    track_trace_point_distances(gentle, 4, metres);
    const int n = track_trace_amber_nodes(gentle, 4, metres, 6.0, amber, 400);
    CHECK(n == 51);   /* nodes 6 m apart over 300 m: 0 .. 300 m is 50 steps and the start */
    int count = 0;
    for (int i = 0; i < n; ++i) count += amber[i];
    CHECK(count == 0);
    /* a step of 5 m in 10 m between points 10 m apart on a flat road: the angle jumps within a node or two */
    const TracePoint sharp[4] = { { 0, 0, 0 }, { 0, 50, 0 }, { 0, 60, 5 }, { 0, 110, 5 } };
    double m2[4];
    track_trace_point_distances(sharp, 4, m2);
    const int n2 = track_trace_amber_nodes(sharp, 4, m2, 6.0, amber, 400);
    count = 0;
    for (int i = 0; i < n2; ++i) count += amber[i];
    CHECK(n2 > 10 && count >= 1 && amber[0] == 0);   /* the first node has no node before: never amber */
    /* a flat road, and the refusals */
    const TracePoint flat[3] = { { 0, 0, 3 }, { 0, 100, 3 }, { 0, 200, 3 } };
    double m3[3];
    track_trace_point_distances(flat, 3, m3);
    CHECK(track_trace_amber_nodes(flat, 3, m3, 6.0, amber, 400) > 0 && amber[5] == 0);
    CHECK(track_trace_amber_nodes(flat, 1, m3, 6.0, amber, 400) == 0 && track_trace_amber_nodes(flat, 3, m3, 0.0, amber, 400) == 0 && track_trace_amber_nodes(flat, 3, m3, 6.0, NULL, 0) == 34);   /* with no room it only counts */
    CHECK(track_trace_amber_nodes(flat, 3, m3, 6.0, amber, 10) == 34 && amber[9] == 0);   /* more nodes than room: counted, the first written */
}

int main(void)
{
    test_range();
    test_rows();
    test_grade();
    test_segment_grade();
    test_amber_nodes();
    return TEST_REPORT();
}
