/* The swap interval chosen for a display's refresh and the game's rate. */
#include "game/platform/vsync_plan.h"
#include "test.h"

int main(void)
{
    CHECK_EQ_INT(vsync_plan_interval(60.0, 60), 1);
    CHECK_EQ_INT(vsync_plan_interval(59.94, 60), 1);
    CHECK_EQ_INT(vsync_plan_interval(60.0, 30), 2);
    CHECK_EQ_INT(vsync_plan_interval(120.0, 60), 2);
    CHECK_EQ_INT(vsync_plan_interval(144.0, 144), 1);
    CHECK_EQ_INT(vsync_plan_interval(240.0, 60), 4);
    CHECK_EQ_INT(vsync_plan_interval(240.0, 120), 2);
    CHECK_EQ_INT(vsync_plan_interval(119.88, 60), 2);
    CHECK_EQ_INT(vsync_plan_interval(144.0, 60), 0);    /* 72 or 48: not 60 */
    CHECK_EQ_INT(vsync_plan_interval(75.0, 60), 0);
    CHECK_EQ_INT(vsync_plan_interval(165.0, 144), 0);
    CHECK_EQ_INT(vsync_plan_interval(60.0, 120), 0);    /* a display slower than the game */
    CHECK_EQ_INT(vsync_plan_interval(0.0, 60), 0);      /* an unknown refresh */
    CHECK_EQ_INT(vsync_plan_interval(-5.0, 60), 0);
    CHECK_EQ_INT(vsync_plan_interval(60.0, 0), 0);
    CHECK_EQ_INT(vsync_plan_interval(61.8, 60), 0);     /* 3 % fast: the software pace stays */
    CHECK_EQ_INT(vsync_plan_interval(143.98, 144), 1);
    CHECK_EQ_INT(vsync_plan_interval(239.76, 60), 4);
    CHECK_EQ_INT(vsync_plan_interval(60.5, 60), 1);     /* under 1 % */
    CHECK_EQ_INT(vsync_plan_interval(60.7, 60), 0);
    /* a present that waited for the vblank takes about a period; one that came back at once (minimised, hidden) does not */
    CHECK_EQ_INT(vsync_kept_pace(16666667, 16666667), 1);
    CHECK_EQ_INT(vsync_kept_pace(16000000, 16666667), 1);
    CHECK_EQ_INT(vsync_kept_pace(33333333, 33333333), 1);   /* two vblanks at 30 */
    CHECK_EQ_INT(vsync_kept_pace(8333334, 16666667), 1);    /* the edge: half a period */
    CHECK_EQ_INT(vsync_kept_pace(2000000, 16666667), 0);    /* returned at once */
    CHECK_EQ_INT(vsync_kept_pace(0, 16666667), 0);          /* the first present: no interval yet */
    CHECK_EQ_INT(vsync_kept_pace(16666667, 0), 0);
    return TEST_REPORT();
}
