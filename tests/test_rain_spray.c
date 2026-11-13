/* The wheels the spray and the dust come from (rain_spray.h): a car's heading by its yaw or its matrix, its rear wheels, and the pace of a car whose physics does not run. */
#include "game/race/rain_spray.h"
#include "test.h"

static void test_a_matrix_car_has_its_wheels_where_it_heads(void)
{
    const int32_t at[3] = { 100 << 16, 0, 200 << 16 };
    int32_t forward[2], wheel[2][3], right[2];
    rain_spray_heading(1, 0, 65536, 0, forward);   /* heading east (+x), yaw 0 */
    CHECK_EQ_INT(forward[0], 65536);
    CHECK_EQ_INT(forward[1], 0);
    rain_spray_wheels(at, forward, wheel, right);
    CHECK_EQ_INT(right[0], 0);                      /* the right of a car heading +x is -z */
    CHECK_EQ_INT(right[1], -65536);
    CHECK(wheel[0][0] < at[0] && wheel[1][0] < at[0]);   /* both behind it, along the road */
    CHECK_EQ_INT(wheel[0][0], wheel[1][0]);
    CHECK(wheel[0][2] > at[2] && wheel[1][2] < at[2]);   /* the left (+z) and the right (-z) either side of it */
    CHECK_EQ_INT(wheel[0][2] - at[2], at[2] - wheel[1][2]);
    rain_spray_heading(1, 0, 0, 0, forward);        /* a degenerate matrix: the yaw's (north, +z) */
    CHECK_EQ_INT(forward[0], 0);
    CHECK_EQ_INT(forward[1], 65536);
    rain_spray_heading(0, 0x4000 << 8, 12345, 678, forward);   /* a car with no matrix: its yaw, a quarter turn */
    CHECK_EQ_INT(forward[0], 65536);
    CHECK_EQ_INT(forward[1], 0);
    rain_spray_heading(1, 0, 32886, 55230, forward);   /* an unnormalised matrix is made a unit one */
    const double len = ((double)forward[0] * forward[0] + (double)forward[1] * forward[1]) / 65536.0 / 65536.0;
    CHECK(len > 0.999 && len < 1.001);
}

static void test_a_car_without_a_speed_goes_by_its_velocity(void)
{
    /* the traffic's physics never runs: its `speed` (0x070) stays 0 while it drives along the road at the pace of its velocity (AL1: about (-4, 16) m/s) */
    CHECK_EQ_INT(rain_spray_speed(20 << 16, 0, 0), 20 << 16);                      /* a car with a speed keeps it (the player's, a rival's) */
    CHECK_EQ_INT(rain_spray_speed(0, -4 * 65536, 16 << 16), (16 << 16) + (4 << 16) / 4); /* 00430cb0's: the larger component and a quarter of the smaller */
    CHECK_EQ_INT(rain_spray_speed(0, 16 << 16, -4 * 65536), (16 << 16) + (4 << 16) / 4); /* whichever way it points */
    CHECK_EQ_INT(rain_spray_speed(0, 0, 0), 0);                                    /* a car that stands is still */
}

int main(void)
{
    test_a_matrix_car_has_its_wheels_where_it_heads();
    test_a_car_without_a_speed_goes_by_its_velocity();
    return TEST_REPORT();
}
