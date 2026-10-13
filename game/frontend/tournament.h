/* The tournament's state in the game state block (0052ad44, 0x5b6 bytes, the
 * race side's RaceState.game_state): a record of 0x95 bytes per player, the
 * 20 lap times at +9, the racers' flags as shorts at +0x5a4. */
#ifndef TNFS_TOURNAMENT_H
#define TNFS_TOURNAMENT_H

#include "game/frontend/race_settings.h"

#include <stdint.h>

enum {
    GAME_STATE_SIZE = 0x5b6,
    GAME_STATE_PLAYERS = 8,
    GAME_STATE_PLAYER = 0x95,
    GAME_STATE_FLAGS = 0x5a4
};

/* 00466a3c: clears each player's record from +9 (the lap times) to its end,
 * the eight racers' flags and the variant. The first 9 bytes of each record
 * stay. */
void tournament_reset(uint8_t game_state[GAME_STATE_SIZE], RaceSettings *s);

#endif
