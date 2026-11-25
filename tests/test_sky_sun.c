/* The sky sun's geometry and its drawing (port glue, no original counterpart): where `sun AZ EL` lands on the frame for a camera, and that the disc brightens the pixels it covers and no others. */
#include "game/viewer/sky_sun.h"
#include "game/sys/palette.h"
#include "test.h"

#include <math.h>
#include <string.h>

static const float DEG = 0.01745329252f;

int main(void)
{
    View view;
    view_init(&view, 0, 0, 640, 480, 90, 90);
    const int32_t identity[9] = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };   /* the camera looks along the world's +z */
    const double focal = (double)view.focal_x;
    SkySunDisc d;

    CHECK(sky_sun_place(&view, identity, 90 * DEG, 0, &d));   /* straight ahead on the horizon: the frame's centre */
    CHECK_EQ_INT(d.x, view.centre_x);
    CHECK_EQ_INT(d.y, view.centre_y);

    CHECK(sky_sun_place(&view, identity, 60 * DEG, 0, &d));   /* 30 degrees toward +x: to the right by focal * tan 30 */
    CHECK(fabs((d.x - view.centre_x) - focal * tan(30 * DEG)) <= 2);
    CHECK_EQ_INT(d.y, view.centre_y);
    CHECK(sky_sun_place(&view, identity, 120 * DEG, 0, &d));  /* and 30 the other way: to the left, as far */
    CHECK(fabs((view.centre_x - d.x) - focal * tan(30 * DEG)) <= 2);

    CHECK(sky_sun_place(&view, identity, 90 * DEG, 20 * DEG, &d));   /* 20 degrees up: above the centre by focal * tan 20 */
    CHECK_EQ_INT(d.x, view.centre_x);
    CHECK(fabs((view.centre_y - d.y) - focal * tan(20 * DEG)) <= 2);

    CHECK(!sky_sun_place(&view, identity, 270 * DEG, 20 * DEG, &d));   /* behind the camera: no disc */
    CHECK(!sky_sun_place(&view, identity, 0, 20 * DEG, &d));           /* square to the side: not in front either */

    CHECK(sky_sun_place(&view, identity, 90 * DEG, 20 * DEG, &d));   /* the radii follow the focal length: 2.2 and 12 degrees */
    CHECK(fabs(d.core - focal * tan(2.2 * DEG)) <= 1);
    CHECK(fabs(d.halo - focal * tan(12.0 * DEG)) <= 1);

    /* a camera turned 90 degrees (the sun that was ahead is now to its left): the matrix is a yaw, +x to +z */
    const int32_t yawed[9] = { 0, 0, 65536, 0, 65536, 0, -65536, 0, 0 };
    CHECK(sky_sun_place(&view, yawed, 0, 0, &d));   /* the sun along +x, the camera looking along +x: ahead */
    CHECK_EQ_INT(d.x, view.centre_x);

    /* the drawing: a grey ramp palette; the disc's centre gets lighter, the corner far from the halo is untouched, nothing is written outside the clip */
    uint8_t palette[768], usable[256], pixels[200 * 200], was[200 * 200];
    for (int i = 0; i < 256; i++) palette[3 * i] = palette[3 * i + 1] = palette[3 * i + 2] = (uint8_t)i, usable[i] = 1;
    palette_usable_set(usable);
    memset(pixels, 40, sizeof pixels);
    memcpy(was, pixels, sizeof pixels);
    const SkySunDisc disc = { 100, 100, 6, 30 };
    sky_sun_draw(&disc, palette, pixels, 200, 0, 0, 199, 99);   /* the clip stops at row 99: the disc's lower half is cut */
    CHECK(pixels[100 * 200 + 100] == 40);   /* row 100 is outside the clip */
    CHECK(pixels[99 * 200 + 100] > 100);    /* inside, at the centre: the disc, much lighter */
    CHECK(pixels[10 * 200 + 10] == 40);     /* far from the halo's reach */
    int lighter = 0, darker = 0;
    for (size_t i = 0; i < sizeof pixels; i++) lighter += pixels[i] > was[i], darker += pixels[i] < was[i];
    CHECK(lighter > 100);
    CHECK_EQ_INT(darker, 0);   /* the sun only adds light */
    for (int y = 100; y < 200; y++)
        for (int x = 0; x < 200; x++) CHECK(pixels[y * 200 + x] == 40);
    return TEST_REPORT();
}
