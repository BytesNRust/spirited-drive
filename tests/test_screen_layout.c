/* The wide screen's pure calculations (port glue, no original counterpart). */
#include "game/viewer/screen_layout.h"
#include "test.h"

#include <string.h>

static void same_layout(const ViewLayout *a, const ViewLayout *b, int row)
{
    CHECK(memcmp(&a[row], &b[row], sizeof *a) == 0);
}

int main(void)
{
    CHECK_EQ_INT(screen_width(SCREEN_4_3), 640);
    CHECK_EQ_INT(screen_width(SCREEN_16_9), 854);
    CHECK_EQ_INT(screen_width(SCREEN_21_9), 1120);
    CHECK_EQ_INT(screen_menu_left(640), 0);
    CHECK_EQ_INT(screen_menu_left(854), 107);
    CHECK_EQ_INT(screen_menu_left(1120), 240);

    CHECK_EQ_INT(screen_fov_x(70, 640), 70);
    CHECK_EQ_INT(screen_fov_x(70, 854), 86);
    CHECK_EQ_INT(screen_fov_y(70, 384, 448, 0), 78);  /* 78.49: the plan's 79 contradicts its own rounding */
    CHECK_EQ_INT(screen_fov_y(70, 480, 480, 5), 75);
    /* the near plane brought in by the focal lengths' ratio for a wider field, never pushed out */
    CHECK_EQ_INT(screen_near_scale(70, 70), 65536);
    CHECK_EQ_INT(screen_near_scale(70, 90), 45889);   /* tan 35 / tan 45 */
    CHECK_EQ_INT(screen_near_scale(90, 70), 65536);

    const ViewLayout *orig = race_view_layouts[1];
    ViewLayout l[VIEW_LAYOUTS];
    screen_layouts(640, l);
    for (int r = 0; r < VIEW_LAYOUTS; r++) {
        if (r == 3) continue;
        same_layout(l, orig, r);
    }
    ViewLayout want3 = orig[3];
    want3.unknown_24 = 448;     /* the box the view is clipped to, or the strip stays; the projection as it was */
    CHECK(memcmp(&l[3], &want3, sizeof want3) == 0);

    screen_layouts(854, l);
    CHECK_EQ_INT(l[0].width, 854);
    CHECK_EQ_INT(l[0].height, 480);
    CHECK_EQ_INT(l[3].width, 854);
    CHECK_EQ_INT(l[3].height, 384);
    CHECK_EQ_INT(l[3].top, 32);
    CHECK_EQ_INT(l[3].unknown_24, 448);
    CHECK_EQ_INT(l[6].left, 64 + 107);
    CHECK_EQ_INT(l[6].width, 512);
    CHECK_EQ_INT(l[11].left, 218 + 107);
    same_layout(l, orig, 12);
    same_layout(l, orig, 13);

    CHECK_EQ_INT(screen_cockpit_scale(640).scale, 65536);
    CHECK_EQ_INT(screen_cockpit_scale(640).crop_top, 0);
    CHECK_EQ_INT(screen_cockpit_scale(854).crop_top, 80);
    CHECK_EQ_INT(screen_cockpit_scale(1120).crop_top, 180);
    CHECK_EQ_INT(screen_cockpit_scale(1120).scale, 114688);

    /* sizes in HudAnchors order: bar start, middle, end, mirror, map, radar, gauge */
    const ScreenPoint sizes[7] = { { 256, 32 }, { 143, 32 }, { 235, 32 }, { 96, 42 }, { 113, 140 }, { 98, 24 },
                                   { 156, 156 } };
    const ScreenPoint mirror640 = { 543, 53 };
    HudAnchors a = screen_hud_anchors(640, sizes, mirror640);
    CHECK_EQ_INT(a.bar_start.x, 0);
    CHECK_EQ_INT(a.bar_start.y, 0);
    CHECK_EQ_INT(a.bar_middle.x, 262);
    CHECK_EQ_INT(a.bar_middle.y, 0);
    CHECK_EQ_INT(a.bar_end.x, 405);
    CHECK_EQ_INT(a.mirror.x, 543);
    CHECK_EQ_INT(a.mirror.y, 53);
    CHECK_EQ_INT(a.map.x, 520);
    CHECK_EQ_INT(a.map.y, 306);
    CHECK_EQ_INT(a.radar.x, 487);
    CHECK_EQ_INT(a.radar.y, 452);
    CHECK_EQ_INT(a.gauge.x, 10);
    CHECK_EQ_INT(a.gauge.y, 289);

    a = screen_hud_anchors(854, sizes, mirror640);
    CHECK_EQ_INT(a.bar_start.x, 0);
    CHECK_EQ_INT(a.bar_middle.x, 262 + 107);
    CHECK_EQ_INT(a.bar_end.x, 854 - 235);
    CHECK_EQ_INT(a.bar_end.y, 0);
    CHECK_EQ_INT(a.mirror.x, 854 - 96);
    CHECK_EQ_INT(a.mirror.y, 38);
    CHECK_EQ_INT(a.map.x, 854 - 113 - 6);
    CHECK_EQ_INT(a.map.y, 480 - 140 - 6);
    CHECK_EQ_INT(a.radar.x, 256 - 98 / 2);
    CHECK_EQ_INT(a.radar.y, HUD_BAR_HEIGHT);
    CHECK_EQ_INT(a.gauge.x, 10);
    CHECK_EQ_INT(a.gauge.y, 289);

    a = screen_hud_anchors(1120, sizes, mirror640);
    CHECK_EQ_INT(a.bar_end.x, 1120 - 235);
    CHECK_EQ_INT(a.mirror.x, 1120 - 96);

    CHECK_EQ_INT(screen_format_nearest(1920, 1080), SCREEN_16_9);
    CHECK_EQ_INT(screen_format_nearest(3440, 1440), SCREEN_21_9);
    CHECK_EQ_INT(screen_format_nearest(1600, 1200), SCREEN_4_3);
    CHECK_EQ_INT(screen_format_nearest(1280, 1024), SCREEN_4_3);
    /* inside the car on a wide screen: the projection the screen's width (the field widened to keep the 640's focal
     * length), DASH_DROP lower with the dash; the clip box the whole width down to the windshield's bottom moved */
    const ViewLayout in854 = screen_cockpit_view(854);
    CHECK_EQ_INT(in854.left + in854.x, 0);
    CHECK_EQ_INT(in854.top + in854.y, DASH_DROP);
    CHECK_EQ_INT(in854.width, 854);
    CHECK_EQ_INT(in854.height, 480);
    CHECK_EQ_INT(in854.left, 0);
    CHECK_EQ_INT(in854.unknown_20, 854);
    CHECK_EQ_INT(in854.unknown_24, race_view_layouts[1][0].unknown_24 + DASH_DROP);
    CHECK_EQ_INT(in854.scale, 65536);
    const ViewLayout in640 = screen_cockpit_view(640);
    CHECK(memcmp(&in640, &race_view_layouts[1][0], sizeof in640) == 0);
    const ViewLayout in1120 = screen_cockpit_view(1120);
    CHECK_EQ_INT(in1120.width, 1120);
    CHECK_EQ_INT(in1120.unknown_20, 1120);

    /* the dash's columns: its middle 2 x DASH_MIDDLE at its own size round the screen's centre, the rest stretched to
     * the edges; at 640 as it is */
    CHECK_EQ_INT(screen_dash_x(640, 0), 0);
    CHECK_EQ_INT(screen_dash_x(640, 639), 639);
    CHECK_EQ_INT(screen_dash_x(640, 321), 321);
    for (int k = 0; k < 2; k++) {
        const int w = k == 0 ? 854 : 1120;
        CHECK_EQ_INT(screen_dash_x(w, 0), 0);
        CHECK_EQ_INT(screen_dash_x(w, w - 1), 639);
        CHECK_EQ_INT(screen_dash_x(w, w / 2), 320);
        CHECK_EQ_INT(screen_dash_x(w, w / 2 + DASH_MIDDLE - 1), 320 + DASH_MIDDLE - 1);
        CHECK_EQ_INT(screen_dash_x(w, w / 2 - DASH_MIDDLE), 320 - DASH_MIDDLE);
        int ordered = 1, last = -1;
        for (int x = 0; x < w; x++) {
            const int sx = screen_dash_x(w, x);
            if (sx < last || sx < 0 || sx > 639) ordered = 0;
            last = sx;
        }
        CHECK(ordered);
    }
    /* its lines: DASH_DROP lower, the roof's first line (under the scoreboard) stretched over the ones above */
    CHECK_EQ_INT(screen_dash_y(0), HUD_BAR_HEIGHT);
    CHECK_EQ_INT(screen_dash_y(HUD_BAR_HEIGHT + DASH_DROP), HUD_BAR_HEIGHT);
    CHECK_EQ_INT(screen_dash_y(HUD_BAR_HEIGHT + DASH_DROP + 1), HUD_BAR_HEIGHT + 1);
    CHECK_EQ_INT(screen_dash_y(479), 479 - DASH_DROP);
    return TEST_REPORT();
}
