/* The split times run each tick (the block is RaceSplits, race_splits.h). */
#ifndef TNFS_RACE_SPLIT_TIMES_H
#define TNFS_RACE_SPLIT_TIMES_H

#include "game/exe/exe_table_const.h"
#include "game/race/race_state.h"

/* 0045b24c: once `progress` (the player's) is past the next checkpoint the
 * tick is kept and the count goes on; the same for the rival's progress
 * unless a time trial. */
void race_splits_pass(RaceState *race, int32_t progress, int32_t rival_progress);

/* 0045b1d0: once both have passed a split, the gap is the rival's tick
 * minus the player's at the last split both passed; a new gap is shown
 * for 0x5a ticks. */
void race_splits_gap(RaceState *race);

enum {
    RACE_SPLIT_TABLES_SIZE = 0x474,     /* 004c584c..004c5cc0 */
    RACE_SPLIT_FACTORS = 0x21c          /* 004c5a68 within it */
};
extern EXE_TABLE_CONST uint8_t RACE_SPLIT_TABLES[RACE_SPLIT_TABLES_SIZE];

/* 0045aab4: at the start, each finishing car's record set to no best and
 * nothing done; the checkpoints from the track's template, a lap apart;
 * the rival's split ticks from the other player's record in a time trial,
 * or from the saved record time scaled by the track's factors when there
 * is none, else cleared; the player's ticks and the gap cleared. */
void race_splits_start(RaceState *race);

/* 0043fbac: the race's lap length and laps, the start-of-race counters and
 * end request cleared, 00509d78 rescaled by the 004fb7f0 row's first word
 * (004c5ce0), the finishing counts by race type, the results order 0..8,
 * the split times (0045aab4). */
void race_laps_start(RaceState *race, int32_t loop_nodes, int32_t laps);

#endif
