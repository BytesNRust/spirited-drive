#include "game/race/car_held.h"

enum { PHYSICS_DRIVING = 2, GEAR_NEUTRAL = -1, AUTOMATIC_DRIVE = 2 };

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
