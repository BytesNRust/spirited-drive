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

/* The car file stem 00457d40 builds for `model` ("WAGON", "MTSUPRA",
 * "TSUPRA", "COPMUST"; upper case) into `stem`: traffic 004c56a0, 100.. the
 * 'm' (004b1174) variant of 004c54d0's racer, 200.. 004c5340's, 400 and up
 * 004c5690's first. Returns 0 for a model past the tables read so far. */
int race_model_stem(int32_t model, char stem[16]);

#endif
