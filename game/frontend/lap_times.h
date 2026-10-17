/* A circuit's lap times screen (0046067c), from its results screen (0045eff4). */
#ifndef TNFS_LAP_TIMES_H
#define TNFS_LAP_TIMES_H

#include "game/frontend/results_screen.h"

#include <stdint.h>

/* 0046067c (ax: the lap record beaten, dx: with ax, play its tune):
 * stats/standlo2.qfs for the track (3..8, "no5w" over all but Rusty
 * Springs on a time trial): the player's laps (00524ea1 of them, two
 * columns past 8: number, time, average on +0x5d), the best one lit, then
 * the track's lap record (lit when ax is 1). Rusty Springs on a time trial
 * without the "r" layout adds the player's top speed and the figures at
 * +0x7d..+0x91 (English: two columns, German: three lines). With the track
 * music on and ax == dx == 1, con2_C16.eas (0051dd14 set). The pointer
 * picks "Done" (0, 0x82), "fin" (400, 0x280) or neither; the arrows move
 * between the two. 1 when "Done" is taken, 0 for "fin", Escape or the
 * timer. Of in, the game state, image, player, German, 0050a5b6, the
 * screen and car names and variant_r are read. */
int32_t lap_times_screen(int32_t record, int32_t tune, const ResultsInputs *in, ResultsState *st, DialogPointer *p,
                         const ResultsOps *ops);

#endif
