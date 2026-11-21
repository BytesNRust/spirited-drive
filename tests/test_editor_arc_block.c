/* dep88: Add curve and Add straight's points (editor/arc_block.h): on the arc, its angle and radius, right and left as mirror images, the
 * default count, a straight's length, and the block going on along a road's heading once pts_paste puts it in. */
#include "arc_block.h"
#include "point_ops.h"
#include "test.h"

#include <math.h>
#include <stdlib.h>

static int near(TracePoint a, double x, double z)
{
    const int ok = fabs(a.x - x) < 1e-6 && fabs(a.z - z) < 1e-6;
    if (!ok) printf("point (%.6f, %.6f), expected (%.6f, %.6f)\n", a.x, a.z, x, z);
    return ok;
}

static void test_arc(void)
{
    TracePoint p[ARC_POINTS_MAX + 1] = { { .x = 0 } };
    CHECK(arc_block(1, 90, 60, 6, p));   /* a quarter to the right, radius 60: the centre is 60 to the right of the start */
    CHECK(near(p[0], 0, 0) && near(p[6], 60, 60));
    for (int i = 0; i <= 6; ++i) CHECK(fabs(hypot(p[i].x - 60, p[i].z) - 60) < 1e-9);   /* every point on the arc */
    for (int i = 1; i < 6; ++i)   /* evenly: 15 degrees apart, equal chords */
        CHECK(fabs(hypot(p[i + 1].x - p[i].x, p[i + 1].z - p[i].z) - hypot(p[1].x - p[0].x, p[1].z - p[0].z)) < 1e-9);
    const PointReadout r = pts_readout(p, 7, 3);
    CHECK(fabs(r.turn_deg - 15) < 1e-9 && fabs(r.radius_m - 60) < 1e-6);   /* to the right, as the Viewport's readout says it */
    TracePoint q[ARC_POINTS_MAX + 1] = { { .x = 0 } };
    CHECK(arc_block(0, 90, 60, 6, q));   /* to the left: the mirror image */
    for (int i = 0; i <= 6; ++i) CHECK(near(q[i], -p[i].x, p[i].z));
    CHECK(arc_block(1, 180, 25, 12, p) && near(p[12], 50, 0));   /* a hairpin ends a diameter across, going back */
    CHECK(!arc_block(1, 0, 60, 6, p) && !arc_block(1, 181, 60, 6, p) && !arc_block(1, 90, 0, 6, p) && !arc_block(1, 90, 60, 0, p)
          && !arc_block(1, 90, 60, ARC_POINTS_MAX + 1, p));
}

static void test_straight(void)
{
    TracePoint p[ARC_POINTS_MAX + 1] = { { .x = 0 } };
    CHECK(straight_block(250, 3, p) && near(p[0], 0, 0) && near(p[1], 0, 250.0 / 3) && near(p[3], 0, 250));
    CHECK(!straight_block(0, 3, p) && !straight_block(STRAIGHT_LENGTH_MAX_M + 1, 3, p) && !straight_block(100, 0, p));
}

static void test_counts(void)
{
    CHECK_EQ_INT(arc_default_count(90, 60), 6);    /* 94 m: 3 chords would do, but 15 degrees each needs 6 */
    CHECK_EQ_INT(arc_default_count(30, 500), 7);   /* 262 m: 40 m chords need 7 */
    CHECK_EQ_INT(arc_default_count(10, 30), 1);
    CHECK_EQ_INT(straight_default_count(250), 3);
    CHECK_EQ_INT(straight_default_count(50), 1);
    CHECK(fabs(arc_length_m(90, 60) - 30 * M_PI) < 1e-9);
}

static void test_goes_on_along_the_road(void)
{
    const TracePoint east[3] = { { .x = 0, .z = 0 }, { .x = 100, .z = 0 }, { .x = 150, .z = 0 } };
    TracePoint arc[7] = { { .x = 0 } };
    CHECK(arc_block(1, 90, 60, 6, arc));
    /* heading east, a quarter right after point 1: it ends 60 on and 60 to the south; the road after it (50 m east) now runs 50 m south */
    TracePoint *q = pts_paste_tangents(east, 3, 1, arc, 7, (TracePoint){ .x = 0, .z = 1 }, arc_end_heading(1, 90));
    CHECK(q != NULL && near(q[7], 160, -60) && near(q[8], 160, -110));
    free(q);
    CHECK(near(arc_end_heading(1, 90), 1, 0) && near(arc_end_heading(0, 180), 0, -1));
    q = pts_paste(east, 3, 1, arc, 7);   /* by its chords instead, the arc would start 7.5 degrees off the road */
    CHECK(q != NULL && !near(q[7], 160, -60));
    free(q);
}

int main(void)
{
    test_arc();
    test_straight();
    test_counts();
    test_goes_on_along_the_road();
    return TEST_REPORT();
}
