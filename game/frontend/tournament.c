#include "game/frontend/tournament.h"

#include <string.h>

enum {
    PLAYER_KEPT = 9,
    /* game state offsets */
    CONTROLS_AT = 0x554, STANDINGS_AT = 0x570, UNKNOWN_580 = 0x580, UNKNOWN_592 = 0x592,
    /* config.dat image offsets (config_dat.h) */
    CONFIG_CONTROLS = 0x5f36, CONTROLS_BYTES = 26
};

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

/* 00466ae4 */
void tournament_new(uint8_t game_state[GAME_STATE_SIZE], RaceSettings *s, const ConfigDat *c, FrontendSession *f)
{
    memcpy(game_state + CONTROLS_AT, c->image + CONFIG_CONTROLS, CONTROLS_BYTES);
    tournament_reset(game_state, s);
    memset(game_state + STANDINGS_AT, 0, 2 * 8);
    memset(game_state + UNKNOWN_580, 0, 2 * 9);
    memset(game_state + UNKNOWN_592, 0, 2 * 9);
    tournament_default_names(game_state, 0);
    f->tournament_on = 0;
    /* the name is strcpy'd: an unended one runs on through the image as in the original */
    const uint8_t *from = c->image;
    uint8_t *to = game_state;
    while ((*to++ = *from++) != 0) {}
}
