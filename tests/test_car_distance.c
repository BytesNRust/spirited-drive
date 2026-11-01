/* port (automatic at a render scale of 2 or 3): the distances a car is cut and drops to its low detail model at, by render scale (car_draw.h) */
#include "game/race/car_draw.h"
#include "test.h"

int main(void)
{
    CHECK_EQ_INT(car_draw_max_z(1), CAR_MAX_Z);   /* the exe's at a scale of 1 */
    CHECK_EQ_INT(car_draw_far_z(1), CAR_FAR_Z);
    CHECK_EQ_INT(car_draw_max_z(2), 2 * CAR_MAX_Z);
    CHECK_EQ_INT(car_draw_far_z(3), 3 * CAR_FAR_Z);
    CHECK_EQ_INT(car_draw_max_z(0), CAR_MAX_Z);   /* out of range: kept to 1..3 */
    CHECK_EQ_INT(car_draw_far_z(9), 3 * CAR_FAR_Z);
    return TEST_REPORT();
}
