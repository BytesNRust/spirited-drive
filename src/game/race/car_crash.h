/* A car leaving the driving model for the crash model (physics 4): its
 * state handed to the collision body (0040dc20, 0040db0c, 0040db84). */
#ifndef TNFS_CAR_CRASH_H
#define TNFS_CAR_CRASH_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 0040db0c: a computer car sliding out of control. */
void car_crash_slide(Car *car, RaceState *race);

/* 0040db84: a crash from a hit (a wall at speed, another car). */
void car_crash_hit(Car *car, RaceState *race);

#endif
