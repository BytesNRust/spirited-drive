/* The split times run each tick (the block is RaceSplits, race_splits.h). */
#ifndef TNFS_RACE_SPLIT_TIMES_H
#define TNFS_RACE_SPLIT_TIMES_H

#include "game/race/race_state.h"

/* 0045b24c: once `progress` (the player's) is past the next checkpoint the
 * tick is kept and the count goes on; the same for the rival's progress
 * unless a time trial. */
void race_splits_pass(RaceState *race, int32_t progress, int32_t rival_progress);

/* 0045b1d0: once both have passed a split, the gap is the rival's tick
 * minus the player's at the last split both passed; a new gap is shown
 * for 0x5a ticks. */
void race_splits_gap(RaceState *race);

#endif
