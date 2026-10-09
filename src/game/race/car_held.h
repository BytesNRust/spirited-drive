/* A car held still (physics model 1): on the grid and when the police pull
 * it over (0046bb78 sets the hold, 00470e70 counts it down). */
#ifndef TNFS_CAR_HELD_H
#define TNFS_CAR_HELD_H

#include "game/race/race_state.h"

/* 0041e3c4: the police stop notices go off, noting the tick if one was up. */
void race_stop_notices_clear(RaceState *race);

#endif
