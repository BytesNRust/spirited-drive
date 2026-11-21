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

int main(void)
{
    test_range();
    test_rows();
    test_grade();
    test_segment_grade();
    return TEST_REPORT();
}
