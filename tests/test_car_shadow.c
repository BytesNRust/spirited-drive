/* A car's shadow box in the view (car_shadow.h): the exe's culls and its corners behind the near plane, and the port's cut of the quad at the plane
 * (psx_shadow) that keeps a close car's shadow. */
#include "game/race/car_shadow.h"
#include "game/viewer/follow_arm.h"
#include "test.h"

#include <stdint.h>
#include <string.h>

enum { HALF_LENGTH = 300, HALF_WIDTH = 150, SHIFT = 9, ONE = 0x10000 };

static const int32_t IDENTITY[9] = { ONE, 0, 0, 0, ONE, 0, 0, 0, ONE };
static const int32_t ORIGIN[3] = { 0, 0, 0 };
static View view;
static ModelView mv;
static Car car;

/* the road under the car: the car's own place, its box half HALF_LENGTH long (along z) and HALF_WIDTH wide (along x), in camera units, each << 10 */
static void contact(void *ctx, Car *c)
{
    (void)ctx;
    c->road_point_heading[0] = c->x, c->road_point_heading[1] = c->y, c->road_point_heading[2] = c->z;
    memset(c->road_forward, 0, sizeof c->road_forward), memset(c->road_side, 0, sizeof c->road_side);
    c->road_forward[2] = HALF_LENGTH << 10, c->road_side[0] = HALF_WIDTH << 10;
}

static void followed(void *ctx, Car *c, const int32_t f[3], const int32_t s[3], const int32_t p[3])
{
    (void)ctx, (void)c, (void)f, (void)s, (void)p;
}

/* the view of a wide field: 1120 x 480 with a horizontal field of 120 degrees (tan 60 = 1.73 to either side, the exe's cone is 1) */
static void setup(int x, int z)
{
    view_init(&view, 0, 0, 1120, 480, 120, 60);
    mv = (ModelView){ &view, ORIGIN, IDENTITY, 0, 0 };
    memset(&car, 0, sizeof car);
    car.x = x << SHIFT, car.y = -(100 << SHIFT), car.z = z << SHIFT;   /* the road 100 units under the lens */
}

static int shadow(const CarShadowOps *ops, int32_t screen[8])
{
    return car_shadow(ops, &car, &mv, 0, 0, screen);
}

static int has_minimum(const int32_t screen[8])
{
    for (int i = 0; i < 8; i++)
        if (screen[i] == INT32_MIN) return 1;
    return 0;
}

/* the exe: a car whose centre is 184 ahead (1.4 m) passes the culls, but its back corners (z -116) are behind the plane and project to INT32_MIN: the
 * quad is lost; with the port's cut every corner is projected */
static void test_close_car(void)
{
    int32_t screen[8];
    const CarShadowOps exe = { NULL, contact, followed, NULL, 0 };
    setup(60, 184);
    CHECK_EQ_INT(shadow(&exe, screen), 1);
    CHECK(has_minimum(screen));   /* the exe's: the corners behind the plane are not points */
    const CarShadowOps cut = { NULL, contact, followed, follow_arm_clip_quad, 0 };
    setup(60, 184);
    CHECK_EQ_INT(shadow(&cut, screen), 1);
    CHECK(!has_minimum(screen));
    for (int k = 0; k < 4; k++) CHECK(screen[2 * k + 1] > view.centre_y);   /* the cut corners at the plane are the nearest ground: low on the screen */
}

/* a car clear of the plane is the same with and without the cut */
static void test_far_car_is_the_exes(void)
{
    int32_t a[8], b[8];
    const CarShadowOps exe = { NULL, contact, followed, NULL, 0 }, cut = { NULL, contact, followed, follow_arm_clip_quad, 0 };
    setup(80, 900);
    CHECK_EQ_INT(shadow(&exe, a), 1);
    setup(80, 900);
    CHECK_EQ_INT(shadow(&cut, b), 1);
    CHECK(memcmp(a, b, sizeof a) == 0);
    setup(80, 0x1901);   /* past the far limit: no shadow either way */
    CHECK_EQ_INT(shadow(&cut, b), 0);
}

/* the exe drops a car whose centre is nearer than 0x20; with the cut the front of its box, still ahead of the plane, keeps the shadow; a car wholly
 * behind the lens has none */
static void test_centre_at_the_lens(void)
{
    int32_t screen[8];
    const CarShadowOps exe = { NULL, contact, followed, NULL, 0 }, cut = { NULL, contact, followed, follow_arm_clip_quad, 0 };
    setup(0, 10);
    CHECK_EQ_INT(shadow(&exe, screen), 0);
    setup(0, 10);
    CHECK_EQ_INT(shadow(&cut, screen), 1);
    CHECK(!has_minimum(screen));
    setup(0, -3000);
    CHECK_EQ_INT(shadow(&cut, screen), 0);
}

/* the cone: the exe drops a car whose centre is outside 45 degrees (|x| > z); in a wide field (here 60 degrees to either side) a car between the two is in view,
 * so with `frustum` its shadow is kept, one past the field's edge is not, and one whose centre is just behind the lens keeps the front of its box */
static void test_the_fields_frustum(void)
{
    int32_t screen[8];
    const CarShadowOps cone = { NULL, contact, followed, follow_arm_clip_quad, 0 }, wide = { NULL, contact, followed, follow_arm_clip_quad, 1 };
    setup(400, 300);   /* 53 degrees off the axis */
    CHECK_EQ_INT(shadow(&cone, screen), 0);
    setup(400, 300);
    CHECK_EQ_INT(shadow(&wide, screen), 1);
    CHECK(!has_minimum(screen));
    setup(2000, 300);  /* 81 degrees: out of the field, its box too */
    CHECK_EQ_INT(shadow(&wide, screen), 0);
    setup(0, -100);    /* the centre behind the lens, the front of the box 200 ahead of it */
    CHECK_EQ_INT(shadow(&cone, screen), 0);
    setup(0, -100);
    CHECK_EQ_INT(shadow(&wide, screen), 1);
    CHECK(!has_minimum(screen));
    setup(0, -1000);   /* the whole box behind */
    CHECK_EQ_INT(shadow(&wide, screen), 0);
    const CarShadowOps exe = { NULL, contact, followed, NULL, 1 };   /* the frustum is the cut's: the exe's culls stay without it */
    setup(400, 300);
    CHECK_EQ_INT(shadow(&exe, screen), 0);
}

int main(void)
{
    test_close_car();
    test_far_car_is_the_exes();
    test_centre_at_the_lens();
    test_the_fields_frustum();
    return TEST_REPORT();
}
