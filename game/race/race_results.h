/* The race's results: the finishing order, the records and the results
 * screen's words (0045af80, 0045b29c). */
#ifndef TNFS_RACE_RESULTS_H
#define TNFS_RACE_RESULTS_H

#include <stdint.h>

/* 0042700c: the short-name index (004c5c84) of car model `model`; the high
 * half of the result is the model's sign, as the original leaves it. */
int32_t race_short_name_index(int16_t model);

#include "game/race/race_state.h"

/* 0045b29c: after a race the finishes go into the game state (summed, the
 * best top speed kept) or are cleared if it did not end normally; the
 * results screen gets the player's place and rank, a rival (the best other
 * total) with its rank, place and short name; past the stages the player's
 * lap times are kept; the record's bests and counts are copied over. */
void race_results_record(RaceState *race);

/* 0045af80: at the race's end the racers' flags into the game state; the
 * humans' records get the race's length mark and their car; the computer
 * cars still racing an estimated finish from how far they got; the order
 * sorted by finish (none last); the player's finish and split ticks kept
 * for the next race's rival; then 0045b29c unless (short)00524e9f is set. */
void race_results_end(RaceState *race);

#endif
