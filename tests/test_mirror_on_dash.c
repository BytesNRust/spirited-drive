/* port: a mirror on the dash (the Warrior's, by the wheel) is not widened on a wide screen; one at the windshield's
 * top is (drive_mirror.h). */
#include "viewer/drive_mirror.h"
#include "viewer/ported_view.h"
#include "test.h"

#include <string.h>

/* the weather's hooks (ported_view.c, not built here): clear */
int ported_view_mirror_weather(const TrackArt *art) { (void)art; return 0; }
int ported_view_mirror_weather_on(void) { return 0; }
void ported_view_mirror_sky(const TrackArt *art, const QuadTarget *target, const View *view, const int32_t matrix[9])
{
    (void)art, (void)target, (void)view, (void)matrix;
}

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

    /* the bumper view's own mirror: the mirror camera's layout's size by a percentage whatever the car, never widened
     * nor on a dash; its layout as large, the same field (the focal length scaled with it) */
    static DriveMirror own;
    drive_mirror_widen(&roof, 1);   /* the screen wide */
    CHECK(drive_mirror_start_own(&own, 150));
    const ViewLayout *l = &race_view_layouts[1][race_view_rows[1][4].layouts[0]];
    CHECK_EQ_INT(own.width, l->width * 3 / 2);
    CHECK_EQ_INT(own.height, l->height * 3 / 2);
    CHECK_EQ_INT(own.own, 1);
    CHECK_EQ_INT(own.wide_layout.width, own.width);
    CHECK_EQ_INT(own.wide_layout.height, own.height);
    CHECK_EQ_INT(own.wide_layout.unknown_20, own.width);
    CHECK_EQ_INT(own.wide_layout.y, l->y * 3 / 2);   /* the horizon as far down the picture */
    drive_mirror_free(&own);
    CHECK(drive_mirror_start_own(&own, 100));
    CHECK_EQ_INT(own.width, l->width);
    CHECK_EQ_INT(own.height, l->height);
    CHECK(own.width > 0 && own.height > 0);
    CHECK_EQ_INT(own.extra, 0);
    CHECK_EQ_INT(own.on_dash, 0);
    CHECK_EQ_INT(own.first_segment, -1);
    drive_mirror_widen(&roof, 0);
    drive_mirror_free(&own);
    CHECK(own.picture == NULL);
    return TEST_REPORT();
}
