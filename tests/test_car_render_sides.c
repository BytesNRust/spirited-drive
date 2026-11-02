/* port: a car beside a wide field's eye (car_render.h's port_reach): the exe's 004591d4 drops a car whose centre is
 * under 0x20 deep or more than twice its depth to the side, while half of it is still on a field wider than its
 * cone; with port_reach the car is drawn while any of it can be in front of the eye, its mark only in front. */
#include "game/race/car_draw.h"
#include "game/race/car_render.h"
#include "test.h"

#include <string.h>

static int bodies, tags;

static void body(void *ctx, Car *car, const int32_t matrix[9], const int32_t position[3])
{
    (void)ctx, (void)car, (void)matrix, (void)position;
    bodies++;
}
static void near_bit(void *ctx, uint32_t object, int on) { (void)ctx, (void)object, (void)on; }
static void vertex(void *ctx, uint32_t object, int32_t index, int32_t out[2])
{
    (void)ctx, (void)object, (void)index;
    out[0] = out[1] = 0;
}
static void point(void *ctx, Car *car, int32_t camera[3], int32_t screen[2])
{
    (void)ctx;
    memcpy(camera, car->camera_position, 3 * sizeof *camera);
    screen[0] = screen[1] = 0;
}
static void trail(void *ctx, const int32_t offset[2]) { (void)ctx, (void)offset; }
static void tag(void *ctx, int32_t x, int32_t y, int32_t slot, int32_t z)
{
    (void)ctx, (void)x, (void)y, (void)slot, (void)z;
    tags++;
}

static const int32_t IDENTITY[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000 };

/* drawn (1) or not, and whether its mark was */
static int drawn_slot(int32_t slot, int32_t x, int32_t z, int32_t reach, int *tagged)
{
    static Car car;
    memset(&car, 0, sizeof car);
    car.index = slot;
    car.camera_position[0] = x, car.camera_position[2] = z;
    const CarRenderOps ops = { NULL, body, near_bit, vertex, point, trail, tag };
    const CarRenderInputs in = { IDENTITY, 0, 0, 1, 8, 0, reach };
    int32_t world[9];
    bodies = tags = 0;
    car_render(&ops, &in, &car, 0, world);
    *tagged = tags;
    return bodies;
}

static int drawn(int32_t x, int32_t z, int32_t reach, int *tagged)
{
    return drawn_slot(1, x, z, reach, tagged);
}

enum { REACH = 300 };

static void test_exe_culls(void)
{
    int t;
    CHECK_EQ_INT(drawn(0, 0x100, 0, &t), 1);
    CHECK_EQ_INT(t, 1);
    CHECK_EQ_INT(drawn(0x300, 0x100, 0, &t), 0);   /* more than 2z to the side */
    CHECK_EQ_INT(drawn(0x80, 0x10, 0, &t), 0);     /* under 0x20 deep */
}

static void test_reach_keeps_a_car_beside(void)
{
    int t;
    CHECK_EQ_INT(drawn(0x300, 0x100, REACH, &t), 1);   /* to the side: drawn, marked */
    CHECK_EQ_INT(t, 1);
    CHECK_EQ_INT(drawn(0x200, -REACH + 1, REACH, &t), 1);   /* its centre just behind the eye, its nose ahead */
    CHECK_EQ_INT(t, 0);                                     /* no mark: its centre is not on the screen */
    CHECK_EQ_INT(drawn(0x200, -REACH - 1, REACH, &t), 0);   /* wholly behind */
    CHECK_EQ_INT(drawn(0, 0x5a01, REACH, &t), 0);           /* the far limit as it was */
    /* the followed car (slot 0): the bumper view's eye inside it, culled as the exe */
    CHECK_EQ_INT(drawn_slot(0, 0, -0x40, REACH, &t), 0);
    CHECK_EQ_INT(drawn_slot(0, 0x300, 0x100, REACH, &t), 0);
}

/* the race's own car draw takes the render scale's distance too (car_draw.h): the cut moves out, a car past the exe's cut shows at a scale of 2 */
static void test_scale_moves_the_cut(void)
{
    int t;
    car_draw_set_distance_scale(1);
    CHECK_EQ_INT(drawn(0, 0x5a01, 0, &t), 0);
    CHECK_EQ_INT(drawn(0, 0x5a01, REACH, &t), 0);
    car_draw_set_distance_scale(2);
    CHECK_EQ_INT(drawn(0, 0x5a01, 0, &t), 1);
    CHECK_EQ_INT(drawn(0, 0x5a01, REACH, &t), 1);
    CHECK_EQ_INT(drawn(0, 2 * 0x5a00 + 1, 0, &t), 0);
    car_draw_set_distance_scale(1);
}

int main(void)
{
    test_exe_culls();
    test_reach_keeps_a_car_beside();
    test_scale_moves_the_cut();
    return TEST_REPORT();
}
