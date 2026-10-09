/* The computer cars' driver (0046f3e8): moves a car's speed towards the one
 * it wants, steers it over the road and pulls it onto the road's plane. */
#ifndef TNFS_AI_DRIVE_H
#define TNFS_AI_DRIVE_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 0046f3e8: `countdown` is the car's ai_countdown as the tick began. */
void ai_drive(Car *car, RaceState *race, int32_t countdown);

/* 0046f354: the fastest sideways speed the car may steer at, signed by
 * `error` (the distance still to go), for a forward speed of `speed`. */
int32_t ai_lane_speed(const Car *car, const RaceState *race, int32_t error, int32_t speed);

/* 0046e3e4: a human's car is near `car` in the road's order. */
int ai_human_near(const RaceState *race, const Car *car);

/* 0046e388: `other`'s node lies from 0x14 nodes behind to 0x36 ahead of `human`'s. */
int ai_in_window(const RaceState *race, const Car *human, const Car *other);

/* 0046a14c: the number (from 1) of the zone `node` is in, or 0; with more
 * than two racers everything counts as in the first. */
int32_t ai_zone(const RaceState *race, int32_t node);

#endif
