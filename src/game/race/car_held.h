/* A car held still (physics model 1): on the grid and when the police pull
 * it over (0046bb78 sets the hold, 00470e70 counts it down). */
#ifndef TNFS_CAR_HELD_H
#define TNFS_CAR_HELD_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 0041e370: the first police stop notice goes up (0046bb78, with the
 * counter 0051ce0c that ends the race once past 1). */
void race_stop_notice_first(RaceState *race);

/* 0041e3c4: the police stop notices go off, noting the tick if one was up. */
void race_stop_notices_clear(RaceState *race);

/* 00470e70 (physics model 1): one tick of the hold; at its end the car
 * drives again, out of gear, an automatic back in D. */
void car_held_tick(Car *car, RaceState *race);

/* 0040d870: a player's car the police are pulling over (+0x176 bit 0)
 * steers toward the police car (cars[2]) and lets the throttle go. */
void car_pulled_over(Car *car, const RaceState *race);

#endif
