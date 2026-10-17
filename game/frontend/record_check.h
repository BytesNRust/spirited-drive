/* A finished segment's records against the player's (0045bad8). */
#ifndef TNFS_RECORD_CHECK_H
#define TNFS_RECORD_CHECK_H

#include <stdint.h>

enum { RECORD_CHECK_IMAGE = 0x5f34, RECORD_CHECK_GAME = 0x5bc };

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

#endif
