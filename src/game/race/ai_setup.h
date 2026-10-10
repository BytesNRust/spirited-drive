/* Setting up the computer cars for a race (004702d4 and what it calls):
 * driver profiles, lane widths, zones, tuning and the players' grid places. */
#ifndef TNFS_AI_SETUP_H
#define TNFS_AI_SETUP_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 00470238: the row of the driver letter table (004c2850, 8 letters a row)
 * the opponents' profiles come from, by the race type: 7 for type 0, 5 or 6
 * by track flag 4 for type 1, 3 or 4 by whether cars 2 and 3 are the same
 * model for type 2, by the first car's model for type 3; anything else keeps
 * `fallback` (the byte the caller had in al). */
int32_t ai_profile_row(const RaceState *race, int32_t fallback);

#endif
