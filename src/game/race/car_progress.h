/* Where a car is along the track: its node (0043ea04), laps and lap times
 * (0043fa3c). */
#ifndef TNFS_CAR_PROGRESS_H
#define TNFS_CAR_PROGRESS_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 0043ea04: walks *node to the nearest road node. Returns 1 when it moved. */
int car_find_node(const Car *car, const RaceState *race, int32_t *node);

/* 0043fa3c: follows the car's node and counts laps. Returns car_find_node's
 * result. */
int car_progress(Car *car, RaceState *race);

#endif
