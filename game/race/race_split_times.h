/* The split times run each tick (the block is RaceSplits, race_splits.h). */
#ifndef TNFS_RACE_SPLIT_TIMES_H
#define TNFS_RACE_SPLIT_TIMES_H

#include "game/race/race_state.h"

/* 0045b24c: once `progress` (the player's) is past the next checkpoint the
 * tick is kept and the count goes on; the same for the rival's progress
 * unless a time trial. */
void race_splits_pass(RaceState *race, int32_t progress, int32_t rival_progress);

#endif
