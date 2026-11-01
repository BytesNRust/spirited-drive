/* port (port_follow_camera): the cars on the rows behind the camera's node (row_cars.h), which the exe's pass draws none on */
#include "game/race/row_cars.h"
#include "test.h"

#include <string.h>

enum { CARS = 5, PHYSICS_SKIPPED = 6 };

static Car cars[CARS];
static Car *list[CARS];
static int drawn[CARS], prepared[CARS];

static void prepare(void *ctx, Car *car, int pass)
{
    (void)ctx, (void)pass;
    prepared[car - cars]++;
}

static void draw(void *ctx, Car *car, int pass)
{
    (void)ctx, (void)pass;
    drawn[car - cars]++;
}

static void unseen(void *ctx, Car *car) { (void)ctx, (void)car; }

static void setup(int32_t n0, int32_t n1, int32_t n2, int32_t n3, int32_t n4)
{
    memset(cars, 0, sizeof cars), memset(drawn, 0, sizeof drawn), memset(prepared, 0, sizeof prepared);
    const int32_t nodes[CARS] = { n0, n1, n2, n3, n4 };
    for (int i = 0; i < CARS; i++) cars[i].draw_node = nodes[i], list[i] = &cars[i];
}

/* the flags by nodes behind the camera: the row's own and those ahead are the exe's, past the flags' reach none */
static void test_flags(void)
{
    uint8_t flags[ROW_CARS_BEHIND];
    setup(95, 100, 105, 3, 39);   /* camera node 100, a ring of 256 nodes */
    car_rows_behind(list, CARS, 64, 100, flags);   /* a ring of 256 nodes */
    CHECK_EQ_INT(flags[5], 1);    /* node 95 */
    CHECK_EQ_INT(flags[0], 0);    /* the camera's own row is the pass's */
    CHECK_EQ_INT(flags[1], 0);
    int set = 0;
    for (int i = 0; i < ROW_CARS_BEHIND; i++) set += flags[i];
    CHECK_EQ_INT(set, 3);         /* nodes 95, 3 (97 behind round the ring) and 39 (61); 105 is ahead of the camera, 251 round the ring */
    CHECK_EQ_INT(flags[97], 1);
    CHECK_EQ_INT(flags[61], 1);
}

/* a row behind draws its cars, prepared then drawn; the followed car, a physics 6 car and another row's are not */
static void test_a_row_behind(void)
{
    RowCarsOps ops = { NULL, prepare, draw, unseen };
    uint8_t flags[ROW_CARS_BEHIND];
    setup(95, 95, 94, 95, 90);
    cars[2].physics = PHYSICS_SKIPPED;
    cars[1].physics = PHYSICS_SKIPPED;
    car_rows_behind(list, CARS, 64, 100, flags);   /* 64 segments: a ring of 256 */
    const RowCarsBehindInputs in = { 100, 64, flags, list, CARS, &cars[3] };
    cars_on_row_behind(&ops, &in, 95);
    CHECK_EQ_INT(drawn[0], 1);
    CHECK_EQ_INT(prepared[0], 1);
    CHECK_EQ_INT(drawn[1], 0);   /* physics 6 */
    CHECK_EQ_INT(drawn[3], 0);   /* the followed car: draw_followed_car's */
    CHECK_EQ_INT(drawn[4], 0);   /* another row */
    cars_on_row_behind(&ops, &in, 94);
    CHECK_EQ_INT(drawn[2], 0);   /* physics 6 */
    cars_on_row_behind(&ops, &in, 90);
    CHECK_EQ_INT(drawn[4], 1);
    cars_on_row_behind(&ops, &in, 100);   /* the camera's row, and ahead of it: not this one's */
    cars_on_row_behind(&ops, &in, 101);
    CHECK_EQ_INT(drawn[0], 1);
}

/* round the ring: the camera's node 3, a car on node 250 of a ring of 256 is 9 behind */
static void test_the_ring(void)
{
    RowCarsOps ops = { NULL, prepare, draw, unseen };
    uint8_t flags[ROW_CARS_BEHIND];
    setup(250, 0, 0, 0, 0);
    for (int i = 1; i < CARS; i++) cars[i].physics = PHYSICS_SKIPPED;
    car_rows_behind(list, CARS, 64, 3, flags);
    CHECK_EQ_INT(flags[9], 1);
    const RowCarsBehindInputs in = { 3, 64, flags, list, CARS, NULL };
    cars_on_row_behind(&ops, &in, 250);
    CHECK_EQ_INT(drawn[0], 1);
}

/* a short ring (20 segments, 80 nodes): the rows ahead of the camera are not taken for rows behind it */
static void test_a_short_ring(void)
{
    uint8_t flags[ROW_CARS_BEHIND];
    setup(50, 0, 0, 0, 0);   /* camera node 40: ten ahead, 70 round the ring behind */
    for (int i = 1; i < CARS; i++) cars[i].physics = PHYSICS_SKIPPED;
    car_rows_behind(list, CARS, 20, 40, flags);
    int set = 0;
    for (int i = 0; i < ROW_CARS_BEHIND; i++) set += flags[i];
    CHECK_EQ_INT(set, 0);
}

int main(void)
{
    test_flags();
    test_a_row_behind();
    test_the_ring();
    test_a_short_ring();
    return TEST_REPORT();
}
