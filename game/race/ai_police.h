/* The police car's chase of the player and the stop (0046bb78). */
#ifndef TNFS_AI_POLICE_H
#define TNFS_AI_POLICE_H

#include "game/race/car.h"
#include "game/race/race_state.h"

#include <stdint.h>

/* 0046bb78: one turn of the computer cars' manager (0046c490) for a police
 * car (+0x174 bit 3) in lane slot `slot` (0..7, as ai_lane_slot): the
 * player's car (cars[0]) pulled over (+0x176 bit 0) once the police car
 * sits right behind it slowly enough; stopped, it is held for params[9] or
 * params[10] ticks by which notice goes up (two of the second kind end the
 * race), set down beside the road and the chase rechecked. Clear of
 * scenery, the police car's steering wish `wish` (three 16.16 words of the
 * manager) leans toward the road's side; close by it marks the player and
 * may start the chase. */
void ai_police_stop(Car *cop, RaceState *race, int32_t slot, int32_t wish[3]);

#endif
