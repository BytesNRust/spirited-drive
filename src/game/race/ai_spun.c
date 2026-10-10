/* Physics model 5 (0046fe34): a computer car's turn dying out while a
 * human is near; once none is, it is set straight on the road again. */
#include "game/race/ai_drive.h"

#include "game/race/ai_lanes.h"

#include <string.h>

enum {
    RACER = 0x4,                  /* flags_174 */
    COP = 0x8,                    /* flags_174 */
    EASE = 0x13333,               /* yaw rate taken off per tick */
    EASE_RACER = 0x23333,
    PHYSICS_PARKED = 6,           /* 00470518 */
    PHYSICS_COMPUTER = 3          /* 0046d79c */
};

void ai_spun_tick(Car *car, RaceState *race)
{
    if (car->flags_174 & COP) ai_release_cops(race, 1);
    const int32_t ease = (car->flags_174 & RACER) ? EASE_RACER : EASE;
    if (car->yaw_rate > 0) {
        car->yaw_rate -= ease;
        if (car->yaw_rate < 0) car->yaw_rate = 0;
    } else if (car->yaw_rate < 0) {
        car->yaw_rate += ease;
    }
    if (car->yaw_rate > 0) car->yaw_rate = 0;
    if (ai_human_near(race, car)) {
        car->ai_target_speed = 0;
        ai_model_tick(car, race);
        return;
    }
    Car *self = race_car_by_handle(race, car->unknown_088);
    if (self != NULL && self->physics == PHYSICS_PARKED) race->ai.traffic_placed--;
    car->yaw_rate = 0;
    car->steer = 0;
    car->slip_steer = 0;
    memcpy(car->matrix, car->road_axes, sizeof car->matrix);
    car->velocity_x = 0;
    car->velocity_y = 0;
    car->velocity_z = 0;
    car->ai_speed = 0;
    car->on_ground = 1;
    if (self != NULL) self->physics = PHYSICS_COMPUTER;  /* port guard: the original's +0x88 always points at a car */
}
