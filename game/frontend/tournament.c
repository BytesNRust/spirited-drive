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

/* 00466b90 */
void tournament_default_names(uint8_t game_state[GAME_STATE_SIZE], int from)
{
    static const char *const NAME[GAME_STATE_PLAYERS] = {
        NULL, "Drifter", "Burnout", "Psycho", "Phreak", "Hacker", "Razor", "Mr. Ice"
    };
    if ((unsigned)from >= GAME_STATE_PLAYERS) return;
    for (int p = from < 1 ? 1 : from; p < GAME_STATE_PLAYERS; p++)
        memcpy(game_state + GAME_STATE_PLAYER * p, NAME[p], strlen(NAME[p]) + 1);
}
