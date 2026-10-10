/* The two zone records (ai.zones, one per human) and which car each follows. */
#ifndef TNFS_AI_ZONES_H
#define TNFS_AI_ZONES_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 0046a29c: car `slot` takes zone `zone` from the car holding it, which
 * gets `old` in exchange; the computer cars of either zone swap with them. */
void ai_swap_zones(RaceState *race, int32_t slot, int32_t zone, int32_t old);

#endif
