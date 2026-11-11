/* Render scale: a viewport for a frame s times as large keeps the fields of view, so
 * the projection of every point lands s times as far from the corner, to a pixel; the box and the sky's box grow with it;
 * and port.cfg's render_scale is read. Needs no exe. */
#include "game/race/view.h"
#include "game/viewer/ported_view.h"
#include "test.h"

static PortedViewport sample(void)
{
    return (PortedViewport){ 0, 60, 640, 360, 96, 0x10000, 0, 60, 640, 360, 70, 0x12000, 1 };
}

static void test_fields_grow(void)
{
    const PortedViewport v = sample();
    for (int s = 1; s <= 3; s++) {
        const PortedViewport b = ported_view_scaled(&v, s);
        CHECK_EQ_INT(b.left, v.left * s);
        CHECK_EQ_INT(b.top, v.top * s);
        CHECK_EQ_INT(b.width, v.width * s);
        CHECK_EQ_INT(b.height, v.height * s);
        CHECK_EQ_INT(b.sky_top, v.sky_top * s);
        CHECK_EQ_INT(b.sky_width, v.sky_width * s);
        CHECK_EQ_INT(b.sky_height, v.sky_height * s);
        CHECK_EQ_INT(b.fov, v.fov);                    /* the fields, the zoom and the near plane stay */
        CHECK_EQ_INT(b.fov_y, v.fov_y);
        CHECK_EQ_INT(b.scale, v.scale);
        CHECK_EQ_INT(b.near_scale, v.near_scale);
        CHECK_EQ_INT(b.render_scale, s);
    }
}

/* the same camera-space points projected by the view of each size: s times the 1x position, give or take a pixel */
static void test_focal_grows(void)
{
    const PortedViewport v = sample();
    View one;
    view_init(&one, v.left, v.top, v.width, v.height, v.fov, v.fov_y);
    long checked = 0;
    for (int s = 2; s <= 3; s++) {
        const PortedViewport b = ported_view_scaled(&v, s);
        View big;
        view_init(&big, b.left, b.top, b.width, b.height, b.fov, b.fov_y);
        for (int x = -3000; x <= 3000; x += 750)
            for (int y = -1500; y <= 1500; y += 500)
                for (int z = 800; z <= 9000; z += 1100) {
                    const int32_t p[3] = { x, y, z };
                    int32_t a[2], c[2];
                    if (!view_project(&one, p, a) || !view_project(&big, p, c)) continue;
                    checked++;
                    CHECK(c[0] >= a[0] * s - s && c[0] <= a[0] * s + s);
                    CHECK(c[1] >= a[1] * s - s && c[1] <= a[1] * s + s);
                }
    }
    CHECK(checked > 100);
}

int main(void)
{
    test_fields_grow();
    test_focal_grows();
    return TEST_REPORT();
}
