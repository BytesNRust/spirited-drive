/* The computer cars' driver (0046f3e8): moves a car's speed towards the one
 * it wants, steers it over the road and pulls it onto the road's plane. */
#ifndef TNFS_AI_DRIVE_H
#define TNFS_AI_DRIVE_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 0046f3e8: `countdown` is the car's ai_countdown as the tick began. */
void ai_drive(Car *car, RaceState *race, int32_t countdown);

/* 0046d5c8: a computer car past the road's side line (road_lateral, signed)
 * loses the part of its speed carrying it further out (by 1.25) and is
 * nudged back by the side row unless already moving back in fast. */
void ai_keep_off_edge(Car *car);

/* 0046f354: the fastest sideways speed the car may steer at, signed by
 * `error` (the distance still to go), for a forward speed of `speed`. */
int32_t ai_lane_speed(const Car *car, const RaceState *race, int32_t error, int32_t speed);

/* 0046e3e4: a human's car is near `car` in the road's order. */
int ai_human_near(const RaceState *race, const Car *car);

/* 0046e388: `other`'s node lies from 0x14 nodes behind to 0x36 ahead of `human`'s. */
int ai_in_window(const RaceState *race, const Car *human, const Car *other);

#endif
