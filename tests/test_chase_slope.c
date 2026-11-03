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
    return TEST_REPORT();
}
