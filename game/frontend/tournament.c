#include "game/frontend/tournament.h"

#include <string.h>

enum { PLAYER_KEPT = 9 };

/* 00466a3c */
void tournament_reset(uint8_t game_state[GAME_STATE_SIZE], RaceSettings *s)
{
    for (int p = 0; p < GAME_STATE_PLAYERS; p++) {
        memset(game_state + GAME_STATE_PLAYER * p + PLAYER_KEPT, 0, GAME_STATE_PLAYER - PLAYER_KEPT);
        memset(game_state + GAME_STATE_FLAGS + 2 * p, 0, 2);
    }
    s->variant = 0;
}
