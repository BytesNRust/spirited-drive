/* The computer cars' manager, run every 4 ticks (0046c490), and its helpers. */
#ifndef TNFS_AI_MANAGER_H
#define TNFS_AI_MANAGER_H

#include "game/race/car.h"

#include <stdint.h>

/* 0046aed0: the three lanes' steering wish `wish` (16.16 words of the
 * manager) loses weight for each lane whose speeds in `first` and `second`
 * (the two rows 0046b0d8 fills) are behind the car's target speed (+0x160)
 * or its speed (+0x15c), against the direction of travel for an oncoming
 * car; then the target speed is held to the middle lane's `first` speed and
 * kept off -0x14ccc (oncoming: under 0x14ccc). */
void ai_weigh_lanes(Car *car, const int32_t first[3], const int32_t second[3], int32_t wish[3]);

#endif
