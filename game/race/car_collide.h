/* Cars running into each other (0040d8e8 and what it calls): the bodies are
 * moved apart and pushed, and a hard enough hit crashes them. */
#ifndef TNFS_CAR_COLLIDE_H
#define TNFS_CAR_COLLIDE_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 0040f384: two cars' bodies, both already holding their cars' state: apart
 * and pushed (0041c944, 0041c098). A light hit hands each car back to its
 * model with its road speed; a hard one (or a player's car in a hard crash
 * already) crashes both. Returns 1 when they touched and pushed. */
int car_cars_collide(Car *a, Car *b, RaceState *race);

/* 0040d714: two cars close enough to touch: both bodies take their cars'
 * state (unless crashed already), then 0040f384; a push at least as loud as
 * the last crash sound is heard (from the car not followed) and shakes both
 * cars' suspensions. Returns 1 when they touched. */
int car_cars_hit(Car *a, Car *b, RaceState *race);

/* 0040d8e8, once a tick: the crash sound's ticks count down (its level
 * drops to 0x6666 once they are out), the computer cars' manager runs every
 * 4 calls (0046c490); then each car in the road's order: a player's is
 * checked for the police stop (0040d870) and, outside the lanes at its
 * node, for scenery it knocks over (0040fc64); a car not gone, with flags_51a
 * bits 1 and 2, is hit against each touchable car after it in the order
 * (0040d714) within both cars' radii in x and z. */
void race_cars_contacts(RaceState *race);

#endif
