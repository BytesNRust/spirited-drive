#include "game/race/car_held.h"

#include "game/sys/fixed_math.h"

enum {
    PHYSICS_DRIVING = 2,
    GEAR_NEUTRAL = -1,
    AUTOMATIC_DRIVE = 2,
    PULLED_OVER = 1,          /* flags_176 */
    POLICE_SLOT = 2,          /* 00509e24 */
    STEER_MOST = 0x140000,
    HALF_TURN = 0x800000,
    TURN = 0x1000000
};

void race_stop_notices_clear(RaceState *race)
{
    if (race->stop_notice[0] != 0 || race->stop_notice[1] != 0) race->stop_notice_tick = race->tick;
    race->stop_notice[0] = 0;
    race->stop_notice[1] = 0;
}

void car_held_tick(Car *car, RaceState *race)
{
    /* the body's +0x84 countdown; +0x88 holds the car's own address */
    car->unknown_148 -= 1;
    if (car->unknown_148 > 0) return;
    car->physics = PHYSICS_DRIVING;
    race_stop_notices_clear(race);
    car->engaged = 0;
    car->gear = GEAR_NEUTRAL;
    if (car->transmission != 0) car->transmission = AUTOMATIC_DRIVE;
    race->unknown_4c538c = 1;
}

void car_pulled_over(Car *car, const RaceState *race)
{
    if (!(car->flags_176[0] & PULLED_OVER)) return;
    const Car *police = race->cars[POLICE_SLOT];
    if (police == NULL) return;  /* port guard: the original reads cars[2] unchecked */
    /* 0042ebbc(dz, dx): the heading to the police car, less the car's own */
    int32_t turn = (int32_t)((uint32_t)((fx_atan2(police->x - car->x, police->z - car->z) << 8) - car->yaw) & 0xffffff);
    if (turn > HALF_TURN) turn -= TURN;
    car->steer = (car->steer >> 1) + (turn >> 1);
    if (car->steer > STEER_MOST)
        car->steer = STEER_MOST;
    else if (car->steer < -STEER_MOST)
        car->steer = -STEER_MOST;
    car->throttle = 0;
}
