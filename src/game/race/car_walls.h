/* The barriers along the road (00430600): which side of the verges the car
 * is on, its surface there, and hitting the walls. */
#ifndef TNFS_CAR_WALLS_H
#define TNFS_CAR_WALLS_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 00430600 */
void car_walls(Car *car, RaceState *race);

/* 00472018: a sound's angle from the listener's heading and its distance. */
void race_sound_position(RaceState *race, const int32_t position[3], int32_t *angle, int32_t *distance);

#endif
