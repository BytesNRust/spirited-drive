/* The slope tool's core (editor/slope_block.h, dep95 step 9): the heights of a run of control points for a grade or a rise, with the grade eased in and out.
 * Pure C: no window, no road. */
#include "slope_block.h"
#include "test.h"

#include <math.h>

static int near_to(double a, double b, double tol)
{
    return fabs(a - b) <= tol;
}

static void test_flat_and_constant(void)
{
    const double d[5] = { 0, 25, 50, 75, 100 };
    double h[5], g = -1;
    CHECK(slope_block(d, 5, 12.0, 0, 0, 0, 0, 0, 0, h, &g));
    for (int i = 0; i < 5; ++i) CHECK(h[i] == 12.0);   /* no grade: the start's height all along */
    CHECK(slope_block(d, 5, 12.0, 0, 0, 0, 10.0, 0, 0, h, &g));   /* 10 % from the first point to the last, no easing */
    for (int i = 0; i < 5; ++i) CHECK(near_to(h[i], 12.0 + 0.1 * d[i], 1e-6));
    CHECK(g == 10.0);
    CHECK(slope_block(d, 5, 0, 0, 0, 0, -4.0, 0, 0, h, NULL) && near_to(h[4], -4.0, 1e-6));   /* a descent; the grade out is optional */
}

static void test_rise_gives_the_grade(void)
{
    const double d[5] = { 0, 25, 50, 75, 100 };
    double h[5], g = 0;
    CHECK(slope_block(d, 5, 0, 0, 0, 1, 5.0, 20.0, 20.0, h, &g));   /* a rise of 5 m with 20 m eased in and 20 m out: the middle 80 m's worth is at G */
    CHECK(near_to(h[4] - h[0], 5.0, 1e-6));
    CHECK(near_to(g, 500.0 / 80.0, 1e-3));   /* the integral of the grade is G * (100 - 20) over 100 */
    CHECK(slope_block(d, 5, 3, 0, 0, 1, -2.5, 0, 0, h, &g) && near_to(h[4], 0.5, 1e-6) && near_to(g, -2.5, 1e-6));
    for (int i = 1; i < 5; ++i) CHECK(h[i] < h[i - 1]);   /* it only goes down */
}

static void test_ease_meets_the_road(void)
{
    const double d[6] = { 0, 0.01, 40, 60, 99.99, 100 };
    double h[6];
    CHECK(slope_block(d, 6, 0, 3.0, -2.0, 0, 8.0, 50.0, 50.0, h, NULL));
    CHECK(near_to((h[1] - h[0]) / 0.01, 0.03, 1e-3));   /* it starts at the grade the road has there ... */
    CHECK(near_to((h[5] - h[4]) / 0.01, -0.02, 1e-3));   /* ... and ends at the one it has after */
    CHECK(h[3] > h[2]);   /* and in between it climbs */
}

static void test_refusals(void)
{
    const double d[4] = { 0, 30, 60, 90 }, bad[4] = { 0, 30, 30, 90 };
    double h[4] = { 7, 7, 7, 7 };
    CHECK(!slope_block(d, 1, 0, 0, 0, 0, 5, 0, 0, h, NULL));   /* one point is no run */
    CHECK(!slope_block(NULL, 4, 0, 0, 0, 0, 5, 0, 0, h, NULL) && !slope_block(d, 4, 0, 0, 0, 0, 5, 0, 0, NULL, NULL));
    CHECK(!slope_block(bad, 4, 0, 0, 0, 0, 5, 0, 0, h, NULL));   /* two points at one place */
    CHECK(!slope_block(d, 4, 0, 0, 0, 0, 5, 50, 50, h, NULL));   /* the easing is longer than the run */
    CHECK(!slope_block(d, 4, 0, 0, 0, 0, 5, -1, 0, h, NULL));
    CHECK(!slope_block(d, 4, 0, 0, 0, 0, SLOPE_GRADE_MAX_PCT + 1, 0, 0, h, NULL));
    CHECK(!slope_block(d, 4, 0, 0, 0, 1, SLOPE_RISE_MAX_M + 1, 0, 0, h, NULL));
    CHECK(!slope_block(d, 4, 0, 0, 0, 0, NAN, 0, 0, h, NULL) && !slope_block(d, 4, NAN, 0, 0, 0, 5, 0, 0, h, NULL));
    CHECK(h[0] == 7 && h[3] == 7);   /* a refusal leaves the heights as they were */
}

int main(void)
{
    test_flat_and_constant();
    test_rise_gives_the_grade();
    test_ease_meets_the_road();
    test_refusals();
    return TEST_REPORT();
}
