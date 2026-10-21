/* port: a mirror on the dash (the Warrior's, by the wheel) is not widened on a wide screen; one at the windshield's
 * top is (drive_mirror.h). */
#include "viewer/drive_mirror.h"
#include "test.h"

#include <string.h>

static void start(DriveMirror *m, int32_t top)
{
    static Cockpit c;
    memset(&c, 0, sizeof c);
    c.dash_points[0].x = 450, c.dash_points[0].y = top;
    c.dash_points[1].x = 590, c.dash_points[1].y = top + 40;
    CHECK(drive_mirror_start(m, &c));
}

int main(void)
{
    static DriveMirror roof, dash;
    start(&roof, 60);
    start(&dash, 250);
    drive_mirror_widen(&roof, 1);
    drive_mirror_widen(&dash, 1);
    CHECK_EQ_INT(roof.on_dash, 0);
    CHECK(roof.extra > 0);
    CHECK_EQ_INT(dash.on_dash, 1);
    CHECK_EQ_INT(dash.extra, 0);
    drive_mirror_free(&dash);
    start(&dash, 250);   /* one made after the screen widened (a cockpit reloaded) stays as it is too */
    CHECK_EQ_INT(dash.extra, 0);
    drive_mirror_widen(&roof, 0);
    drive_mirror_widen(&dash, 0);
    drive_mirror_free(&roof);
    drive_mirror_free(&dash);
    return TEST_REPORT();
}
