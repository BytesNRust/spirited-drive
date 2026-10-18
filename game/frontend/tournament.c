#include "game/frontend/tournament.h"

#include <string.h>

enum {
    PLAYER_KEPT = 9, TOTAL_AT = 0x65,
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

/* 0045b6c8 */
void tournament_results_order(const uint8_t game_state[GAME_STATE_SIZE], int32_t finish[GAME_STATE_PLAYERS],
                              TournamentOrderWork *work)
{
    for (int p = 0; p < GAME_STATE_PLAYERS; p++) {
        const uint8_t *const t = game_state + GAME_STATE_PLAYER * p + TOTAL_AT;
        work->totals[p] = (int32_t)((uint32_t)t[0] | (uint32_t)t[1] << 8 | (uint32_t)t[2] << 16 | (uint32_t)t[3] << 24);
    }
    int32_t best = 0, best_total = 0;
    for (int k = 0; k < GAME_STATE_PLAYERS; k++) {
        best = 0, best_total = work->totals[0];
        for (int p = 0; p < GAME_STATE_PLAYERS; p++) {
            if (work->totals[p] < best_total) best_total = work->totals[p], best = p;
        }
        finish[k] = best;
        work->totals[best] = TOURNAMENT_TAKEN;
    }
    work->last = best, work->last_total = best_total;
}

/* 0045b73c */
int32_t tournament_leads(const uint8_t game_state[GAME_STATE_SIZE], int32_t player)
{
    int32_t top = 0;
    for (int p = 0; p < GAME_STATE_PLAYERS; p++) {
        const int16_t points = (int16_t)(game_state[STANDINGS_AT + 2 * p] | game_state[STANDINGS_AT + 2 * p + 1] << 8);
        if (top < points) top = points;
    }
    const uint8_t *const mine = game_state + STANDINGS_AT + 2 * player;
    return (int16_t)(mine[0] | mine[1] << 8) == top;
}

/* 0045ae80 */
int32_t tournament_champion(const uint8_t game_state[GAME_STATE_SIZE], int32_t player)
{
    const uint8_t *const mine = game_state + STANDINGS_AT + 2 * player;
    const int16_t points = (int16_t)(mine[0] | mine[1] << 8);
    for (int p = 0; p < GAME_STATE_PLAYERS; p++)
        if (points < (int16_t)(game_state[STANDINGS_AT + 2 * p] | game_state[STANDINGS_AT + 2 * p + 1] << 8)) return 0;
    return 1;
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

/* 00432720 */
void race_settings_type_step(RaceSettings *s, Unlocks *u, FrontendSession *f, uint8_t game_state[GAME_STATE_SIZE],
                             const ConfigDat *c, int key, TournamentAsk ask, void *ctx)
{
    enum { KEY_RIGHT = 0x4d00 };
    if (f->tournament_on) {
        if ((int16_t)ask(f->german, ctx) == 0) return;
        s->race_type = RACE_HEAD_TO_HEAD;
        tournament_new(game_state, s, c, f);
        f->tournament_on = 0;  /* 00432767, already 0 from 00466ae4 */
    } else {
        const int right = key == KEY_RIGHT;
        s->race_type = (int16_t)((s->race_type + (right ? 1 : 3)) % 4);
        if (f->flag_52b312) {
            if (s->race_type == RACE_TIME_TRIAL) s->race_type = right ? RACE_HEAD_TO_HEAD : RACE_TOURNAMENT;
            if (s->race_type == RACE_HEAD_TO_HEAD && f->unknown_4f81d2 > 2)
                s->race_type = right ? RACE_SINGLE : RACE_TOURNAMENT;
        }
    }
    race_settings_type_changed(s, u, f);
}
