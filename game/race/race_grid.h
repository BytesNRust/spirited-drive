/* The starting grid places (004fba50, RaceState.grid) as 004275f8 sets
 * them before the cars are entered (0042748c). */
#ifndef TNFS_RACE_GRID_H
#define TNFS_RACE_GRID_H

#include "game/race/race_state.h"

/* 0042748c: `race_type` and `variant` are the frontend's words (00524e5d,
 * 00524e83; 0042702c copies them to race_type and track_variant). Types 2
 * and 3 with variant 0: places {7,6,2,3,4,5,1,0} (the player last), or in
 * a tournament with points (game state 0052b2d6..0052b2e6 not all 0) by
 * the standings (0052b2b4, the most points first); another variant: by
 * the last results (004fa030). Other types: slots 0 and 1 places 7 and 6. */
void race_grid(RaceState *race, int16_t race_type, int16_t variant);

#endif
