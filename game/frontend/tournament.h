/* The tournament's state in the game state block (0052ad44, 0x5b6 bytes, the
 * race side's RaceState.game_state): a record of 0x95 bytes per player, the
 * 20 lap times at +9, the racers' flags as shorts at +0x5a4. */
#ifndef TNFS_TOURNAMENT_H
#define TNFS_TOURNAMENT_H

#include "game/frontend/config_dat.h"
#include "game/frontend/race_settings.h"

#include <stdint.h>

enum {
    GAME_STATE_SIZE = 0x5bc,
    GAME_STATE_PLAYERS = 8,
    GAME_STATE_PLAYER = 0x95,
    GAME_STATE_FLAGS = 0x5a4
};

/* 00466a3c: clears each player's record from +9 (the lap times) to its end,
 * the eight racers' flags and the variant. The first 9 bytes of each record
 * stay. */
void tournament_reset(uint8_t game_state[GAME_STATE_SIZE], RaceSettings *s);

/* 0045b73c: 1 when the player's tournament points (the shorts at +0x570)
 * are the most of the eight, counted from 0 (so 0 when all are negative). */
int32_t tournament_leads(const uint8_t game_state[GAME_STATE_SIZE], int32_t player);

/* 00466b90: the computer players' names (004b1858..) with their NUL at the
 * start of the records of players max(from, 1)..7; nothing when from is
 * past 7 or negative (the original's unsigned test). Player 0's never. */
void tournament_default_names(uint8_t game_state[GAME_STATE_SIZE], int from);

/* 00466ae4: a new tournament. The controls (config.dat's 26 bytes at
 * 0052ad26) go to +0x554, 00466a3c, the shorts at +0x570 (8), +0x580 (9) and
 * +0x592 (9) are cleared, 00466b90 from player 1, f->tournament_on = 0, and
 * config.dat's player name is strcpy'd to player 0's record. */
void tournament_new(uint8_t game_state[GAME_STATE_SIZE], RaceSettings *s, const ConfigDat *c, FrontendSession *f);

/* The Yes / No question 00465f4c asks with the text for the language
 * ("quit the tournament?", 004af7cc German, 004af7e0 English); not 0 is yes. */
typedef int (*TournamentAsk)(int german, void *ctx);

/* 00432720: the race type arrows (key 0x4d00 right, any other left). During a
 * tournament: asks to quit it, and on yes the race type becomes 1 and a new
 * tournament (00466ae4) clears it; on no nothing at all. Otherwise the type
 * steps mod 4; in a 0052b312 game 0 is skipped and so is 1 when
 * f->unknown_4f81d2 is above 2. Then 00432974. */
void race_settings_type_step(RaceSettings *s, Unlocks *u, FrontendSession *f, uint8_t game_state[GAME_STATE_SIZE],
                             const ConfigDat *c, int key, TournamentAsk ask, void *ctx);

#endif
