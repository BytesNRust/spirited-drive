/* The race's results (race_results.h). */
#include "game/race/race_results.h"

enum { SHORT_NAME_ORDER_SIZE = 16 };

/* 00426640: the short-name index (004c5c84: RX-7, NSX, SUPRA, 911, ZR-1,
 * VIPER, 512TR, DIABLO) of each car model, then 8..15 as themselves. */
static const int16_t SHORT_NAME_ORDER[SHORT_NAME_ORDER_SIZE] = { 2, 7, 3, 4, 6, 5, 1, 0, 8, 9, 10, 11, 12, 13, 14, 15 };

int32_t race_short_name_index(int16_t model)
{
    /* cwde, then ax from the table: the high half is the model's sign */
    const uint32_t high = model < 0 ? 0xffff0000u : 0;
    const int16_t index = model >= 0 && model < SHORT_NAME_ORDER_SIZE ? SHORT_NAME_ORDER[model] : 0;  /* port guard: the original reads its stack past the table */
    return (int32_t)(high | (uint16_t)index);
}

#include "game/race/race_state.h"

#include <string.h>

enum {
    GAME_PLAYER = 0x95,           /* game_state bytes a player */
    GAME_PLAYERS = 8,
    GAME_LAP_TIMES = 9,           /* game_state player +9: 20 lap times */
    GAME_LAP_SLOTS = 20,
    GAME_FINISH = 0x59,           /* +0x59: the last finish, +0x5d record +0x1c8, +0x61 top speed */
    GAME_SPEC_1C8 = 0x5d,
    GAME_TOP_SPEED = 0x61,
    GAME_TOTAL = 0x65,            /* the finishes added up */
    GAME_BEST_SPEED = 0x6d,
    RACE_ENDED = 1,               /* end_request after a normal finish */
    RACE_QUIT = 0x10,
    LAST_STAGE_TRACK = 2,
    SHORT_LAP = 0x3c,             /* lap times below this are kept as 0 */
    NO_HUMAN_PLACE = 8
};

static int32_t game_word(const RaceState *race, int64_t at)
{
    int32_t v = 0;
    if (at >= 0 && at + 4 <= (int64_t)sizeof race->game_state) memcpy(&v, race->game_state + at, sizeof v);
    return v;
}

static void set_game_word(RaceState *race, int64_t at, int32_t v)
{
    if (at >= 0 && at + 4 <= (int64_t)sizeof race->game_state) memcpy(race->game_state + at, &v, sizeof v);
}

static int32_t player_word(const RaceState *race, int64_t at)
{
    int32_t v = 0;
    if (at >= 0 && at + 4 <= (int64_t)sizeof race->players) memcpy(&v, (const uint8_t *)race->players + at, sizeof v);
    return v;
}

static int32_t order_at(const RaceState *race, int32_t k)
{
    return k >= 0 && k < 9 ? race->results_order[k] : 0;  /* port guard: 9 slots */
}

/* The finishes into the game state, or cleared when the race was not
 * finished normally. */
static void add_finishes(RaceState *race)
{
    if (race->end_request != RACE_ENDED) {
        for (int64_t p = 0; p < GAME_PLAYERS; p++) {
            set_game_word(race, p * GAME_PLAYER + GAME_FINISH, 0);
            set_game_word(race, p * GAME_PLAYER + GAME_TOTAL, 0);
            set_game_word(race, p * GAME_PLAYER + GAME_TOP_SPEED, 0);
            set_game_word(race, p * GAME_PLAYER + GAME_BEST_SPEED, 0);
        }
        return;
    }
    for (int64_t i = 0; i < race->racers; i++) {
        const int64_t g = i * GAME_PLAYER, r = i * (int64_t)sizeof(PlayerRecord);
        set_game_word(race, g + GAME_FINISH, player_word(race, r + 0x1c4));
        set_game_word(race, g + GAME_TOP_SPEED, player_word(race, r + 0x1cc));
        const int32_t finish = game_word(race, g + GAME_FINISH);
        set_game_word(race, g + GAME_TOTAL, finish > 0 ? game_word(race, g + GAME_TOTAL) + finish : 0);
        if (game_word(race, g + GAME_BEST_SPEED) < game_word(race, g + GAME_TOP_SPEED))
            set_game_word(race, g + GAME_BEST_SPEED, game_word(race, g + GAME_TOP_SPEED));
    }
}

void race_results_record(RaceState *race)
{
    add_finishes(race);
    race->results_shown_place = NO_HUMAN_PLACE;
    for (int32_t k = 0; k < race->racers; k++) {
        const int32_t slot = order_at(race, k);
        if (slot == race->player) race->results_player_place = (int16_t)k;
        else if (race->playback != 0 && slot < race->humans && k < race->results_shown_place)
            race->results_shown_place = (int16_t)k;
    }
    int32_t rank[GAME_PLAYERS] = { 0 };
    for (int32_t i = 0; i < race->racers && i < GAME_PLAYERS; i++)  /* port guard: 8 on the original's stack */
        for (int32_t j = 0; j < race->racers; j++)
            if (game_word(race, (int64_t)i * GAME_PLAYER + GAME_TOTAL) > game_word(race, (int64_t)j * GAME_PLAYER + GAME_TOTAL))
                rank[i]++;
    race->results_player_rank = (int16_t)rank[race->player & (GAME_PLAYERS - 1)];
    for (int32_t k = 0; k < race->racers && k < 9; k++) race->results_order_short[k] = (int16_t)race->results_order[k];
    race->results_rival = (int16_t)(race->player == 0);
    const int32_t rivals = race->playback != 0 ? race->humans : race->racers;
    for (int32_t i = 0; i < rivals; i++)
        if (game_word(race, (int64_t)i * GAME_PLAYER + GAME_TOTAL) <
                game_word(race, (int64_t)race->results_rival * GAME_PLAYER + GAME_TOTAL) && i != race->player)
            race->results_rival = (int16_t)i;
    race->results_rival_rank = (int16_t)rank[race->results_rival & (GAME_PLAYERS - 1)];
    const int32_t model = player_word(race, (int64_t)race->results_rival * (int64_t)sizeof(PlayerRecord) + 0x1d4) >> 16;
    race->results_rival_name = (int16_t)race_short_name_index((int16_t)model);
    for (int32_t k = 0; k < race->racers; k++)
        if (order_at(race, k) == race->results_rival) {
            race->results_shown_place = (int16_t)k;
            break;
        }
    const int64_t g = (int64_t)race->player * GAME_PLAYER, r = (int64_t)race->player * (int64_t)sizeof(PlayerRecord);
    if ((int16_t)race->frontend_track > LAST_STAGE_TRACK) {
        for (int64_t c = 0; c < GAME_LAP_SLOTS; c++) set_game_word(race, g + GAME_LAP_TIMES + 4 * c, 0);
        if (race->end_request != RACE_QUIT)
            for (int64_t c = 0; c < (race->frontend_laps_word >> 16); c++) {
                int32_t t = player_word(race, r + 0xc + 4 * c);
                if (c > 0) {
                    t -= player_word(race, r + 8 + 4 * c);
                    if (t < SHORT_LAP) t = 0;
                }
                set_game_word(race, g + GAME_LAP_TIMES + 4 * c, t);
            }
    }
    static const int16_t COPY[][2] = { { 0x71, 0x1b8 }, { 0x75, 0x1bc }, { GAME_SPEC_1C8, 0x1c8 }, { 0x7d, 0x1a0 },
                                       { 0x81, 0x1a4 }, { 0x85, 0x1ac }, { 0x89, 0x1a8 }, { 0x91, 0x1b0 }, { 0x8d, 0x1b4 } };
    for (size_t i = 0; i < sizeof COPY / sizeof COPY[0]; i++)
        set_game_word(race, g + COPY[i][0], player_word(race, r + COPY[i][1]));
}
