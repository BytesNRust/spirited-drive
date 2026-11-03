/* port: the chase eye's lift from the road's slope (race_chase.h). Behind the car, the exe's: uphill is halved. Ahead of it (the look back), uphill is the
 * whole slope: the half left the camera low under a climbing road. */
#include "game/race/race_chase.h"
#include "test.h"

#include <stdlib.h>

enum { UP = 0x100, DOWN = 0x3f00, FULL = 0x600, DISTANCE = 20 << 16 };   /* 14-bit slope words: +5.6 and -5.6 degrees */

int main(void)
{
    const int32_t behind_up = race_chase_slope_lift(UP, UP, 0, FULL, -DISTANCE);
    const int32_t ahead_up = race_chase_slope_lift(UP, UP, 0, FULL, DISTANCE);
    const int32_t behind_down = race_chase_slope_lift(DOWN, DOWN, 0, FULL, -DISTANCE);
    const int32_t ahead_down = race_chase_slope_lift(DOWN, DOWN, 0, FULL, DISTANCE);
    CHECK(behind_up < 0 && ahead_up > 0);                       /* behind lower, ahead higher */
    CHECK(llabs(ahead_up) * 10 >= llabs(behind_up) * 18);        /* the whole slope ahead, the exe's half behind */
    CHECK(behind_down > 0 && ahead_down < 0);
    CHECK_EQ_INT(ahead_down, -behind_down);                      /* downhill was never halved: symmetric */
    CHECK_EQ_INT(race_chase_slope_lift(UP, UP, 0, FULL, 0), 0);  /* the eye above the car: no lift */
    /* the look back's eye keeps the flat chase's height above the road at its own place, up or down the slope */
    const int32_t base_y = 100 << 16, flat_y = 4 << 16, road_car = 98 << 16;
    CHECK_EQ_INT(race_chase_look_back_y(road_car, road_car, base_y, flat_y), base_y + flat_y);                         /* flat: the chase's own eye */
    CHECK_EQ_INT(race_chase_look_back_y(road_car + (3 << 16), road_car, base_y, flat_y), base_y + flat_y + (3 << 16));  /* the road rises under the eye: so does it */
    CHECK_EQ_INT(race_chase_look_back_y(road_car - (3 << 16), road_car, base_y, flat_y), base_y + flat_y - (3 << 16));  /* it falls: so does the eye */
    return TEST_REPORT();
}
