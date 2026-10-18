/* A finished segment's records against the player's (0045bad8). */
#ifndef TNFS_RECORD_CHECK_H
#define TNFS_RECORD_CHECK_H

#include <stdint.h>

enum { RECORD_CHECK_IMAGE = 0x5f34, RECORD_CHECK_GAME = 0x5bc, RECORD_CHECK_TIMES = 7, RECORD_CHECK_NONE = 0x14 };

/* 0045bad8 (no arguments; eax the answer): on config.dat's image (from
 * 00524df0) for the track (00524e95) and segment (00524e83 - 1, 2 when 0),
 * unless the player's car (00524e8b) is 0xb: the player's (004fbb08)
 * segment time (game state +0x59), above 0 and below the segment's best
 * (+0xd0), becomes it: the name strcpy'd to +0xb5, the time and top speed
 * (+0x61) to +0xd0 / +0xd4, the game state's shorts 0052b27e, 0052b27a,
 * 0052b250 to +0xc4, +0xc8, +0xd8, and +0xcc = 1; a top speed above the
 * segment's (+0x149) likewise to the second record (+0x12a name, +0x145,
 * +0x149, +0x139, +0x13d, +0x14d, +0x141 = 1). 1 for the first, 2 for the
 * second, 3 for both. */
int32_t record_check(uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME], int32_t player);

/* 0045b908 (a circuit's): the player's best lap (the first smallest of the
 * 20 at game state +9 above 0) below the track's lap record (+0xd0, its
 * first row) takes it as 0045bad8's first record does, with the best top
 * speed (+0x6d); 1 when it did. */
int32_t record_check_lap(uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME], int32_t player);

/* 0045c2d0 (a circuit's): the player's best top speed (+0x6d) above the
 * track's top speed record (+0x650) takes it (+0x631 name, +0x64c the
 * total +0x65, +0x650, +0x640, +0x644, +0x654, +0x648 = 1); 1 when it did. */
int32_t record_check_top(uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME], int32_t player);

/* 0045be50: unless the car is 0xb, the player's total (+0x65), above 0,
 * below one of the track's ten best (+0x1ba of rows 0x27 apart, a circuit's
 * 0x186 x the difficulty (00524ea3) on) goes in before the first it beats:
 * the rows from there move down one (the tenth dropped), the row takes the
 * name, the total, the best top speed (+0x6d) and the car as the records
 * do. A stage's also keeps the run's three top speeds and totals (times:
 * 004fa0cc, from [1] and [4]) at +0x664 / +0x658. The row, or
 * RECORD_CHECK_NONE (ax; the original's high half is its stack's). */
int32_t record_check_totals(uint8_t image[RECORD_CHECK_IMAGE], const uint8_t game[RECORD_CHECK_GAME], int32_t player,
                            const int32_t times[RECORD_CHECK_TIMES]);

#endif
