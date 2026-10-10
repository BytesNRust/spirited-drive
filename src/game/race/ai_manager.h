/* The computer cars' manager, run every 4 ticks (0046c490), and its helpers. */
#ifndef TNFS_AI_MANAGER_H
#define TNFS_AI_MANAGER_H

#include "game/race/car.h"
#include "game/race/race_state.h"

#include <stdint.h>

/* 0046aed0: the three lanes' steering wish `wish` (16.16 words of the
 * manager) loses weight for each lane whose speeds in `first` and `second`
 * (the two rows 0046b0d8 fills) are behind the car's target speed (+0x160)
 * or its speed (+0x15c), against the direction of travel for an oncoming
 * car; then the target speed is held to the middle lane's `first` speed and
 * kept off -0x14ccc (oncoming: under 0x14ccc). */
void ai_weigh_lanes(Car *car, const int32_t first[3], const int32_t second[3], int32_t wish[3]);

/* 0046a95c: moving from lane slot `from` to `to`, the target speed (+0x160)
 * is held to `row` (the manager's first speed row) at the new lane's side
 * (row[2] for a higher slot, row[0] for a lower one) when there is room
 * past `ahead` (the car ahead, NULL for none; crashed counts as none): its
 * node gap x 6 x the car's sideways speed beats the sideways distance still
 * to go x the speed gap x 1.2. Otherwise, or staying, the middle row[1].
 * Kept off -0x14ccc as 0046aed0 (oncoming: below 0x14ccc). */
void ai_hold_lane_speed(Car *car, const RaceState *race, const Car *ahead, int32_t from, int32_t to,
                        const int32_t row[3]);

#endif
