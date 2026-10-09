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

/* 0040ddb0: back from the body to the car, in physics model `physics`. */
void car_crash_end(Car *car, RaceState *race, int32_t physics);

/* 0040df24: the body came to rest upright: back to the driving model,
 * the car's turns read off the body. */
void car_crash_settle(Car *car, RaceState *race);

/* 0040f73c: the body's speed along the road (road_axes' forward row). */
int32_t car_body_road_speed(const Car *car);

/* 0040f8a8: a computer car's first hit takes most of its spin and kicks it
 * up and back along the road. */
void car_crash_kick(Car *car, RaceState *race);

#endif
