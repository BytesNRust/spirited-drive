/* Which car model each slot of a race gets (00457870, run by 004275f8
 * before the cars are entered). RaceState.models[slot] (004fba94) then
 * names the files 00457d40 loads: below 100 a traffic car (004c56a0), 100
 * to 199 and 200 to 399 a racer (the two car file sets, 004c54d0 and
 * 004c5340), 400 and up the police car. */
#ifndef TNFS_RACE_MODELS_H
#define TNFS_RACE_MODELS_H

#include "game/race/race_state.h"

/* 00457870: the humans' models from their entries (+200, or the set of
 * 004fb787 for the other humans of a race of type 2 and up); the
 * opponents' from the race type (type 2 the other entry's car, 004c5628
 * when it is car 8, or the head to head pick 00426fec(004fbad0); type 3
 * three cars by the player's, in turn); in a race of type 0 or 1 the
 * police car (400 in slot 2, 004fbab8 = 1) and the traffic models drawn
 * without repeats from race_random. Where the original reads unset stack
 * words or loops for ever the port leaves the models as they are. */
void race_models(RaceState *race);

/* 00426fec (watcall (pick), from 00457870 and 0042702c): the head to head
 * opponent's model for `pick` (00426620: 7, 6, 0, 2, 3, 5, 4, 1, then 8..15
 * as themselves); the high half is the pick's sign, as the original leaves
 * it. Port guard: 0 past the table (the original reads its stack). */
int32_t race_head_to_head_model(int16_t pick);

#endif
