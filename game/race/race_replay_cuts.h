/* The replay director's cuts, chosen from the race's notes. */
#ifndef TNFS_RACE_REPLAY_CUTS_H
#define TNFS_RACE_REPLAY_CUTS_H

#include "game/race/race_state.h"

#include <stdint.h>

/* 0044d7d8 (no arguments; end the recording's last clock, 005120c8): the
 * cuts 00512d94 built from the notes 00512f94 ((second << 8) | 0x80 +
 * level, the loudest of each second). Of the first min(end / 60, 0x1e0)
 * notes, 30 rounds each pick the loudest (the last of equals) whose second
 * is at least the start ((end - 0x7080) / 60 + 60, or -2 when that is
 * below -2); its second goes to 00512efc[k] (written even when nothing
 * above 0x80 is found), and when its level is above 0x80 k counts up and
 * the note drops by 0x80. Then every note of the first min(end / 60,
 * 0x1e0) with a level of 1..0x7f gets 0x80 (also ones never picked). The
 * picks sorted up (bubble passes while one swaps, at most k). Per pick p
 * with p - 2 at least the start: a cut from (p - 2) x 60 to (p + 5) x 60,
 * each rounded toward 0 to 4, the clock at least 0 and the end at most
 * end, its word the level of note p; when it starts before the previous
 * cut's end, that one takes its end and the higher word and the row is
 * left written but not counted. No cut: one, 0 to end, word 200. Notes
 * past the table (a second of 0x1e0 or more): the original reads past it,
 * the port takes 0. */
void race_replay_cuts_build(RaceState *race, int32_t end);

#endif
