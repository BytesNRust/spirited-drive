#include "game/frontend/menu_race.h"

#include "game/frontend/palette_put.h"

#include <stdio.h>
#include <string.h>

/* config.dat's image */
enum {
    SETUP = 9, SETUP_SIZE = 0xac, RACE_TYPE = 0x6d, SEGMENT = 0x93, TRACK = 0x95, LAPS = 0xb1, DIFFICULTY = 0xb3,
    BEST = 0x1ba, BEST_STRIDE = 0x186, GHOST = 0x670, GHOST_STRIDE = 0x190, TRACK_RECORDS = 0xa6b,
    CONTROLS = 0x5f36, CONTROLS_SIZE = 0x1a, LEVEL = 0x5f50
};
/* the game state */
enum {
    SETUP_KEPT = 0x4a8, CONTROLS_KEPT = 0x554, RUN = 0x532, TRACK_KEPT = 0x534, PLAYER = 0x95, LAP = 0x5d,
    SPEED = 0x61, TOTAL = 0x65, TRACK_TOTAL = 0x69, TOP = 0x6d, WON = 0x592, TRACKS = 9, TYPE_KEPT = 0x5b4,
    RACED = 0x5ba
};
enum { ARGS = 20, ARG = 10, FINISHED = 1, ENDED_4 = 4, TOURNAMENT = 3, CUP = 2 };

/* 00426660 */
static const char *const STEM[9] = { "cy", "cl", "al", "tr1", "tr2", "tr6", "tr3", "tr7", "tr4" };

static int16_t g16(const uint8_t *at) { return (int16_t)(uint16_t)(at[0] | at[1] << 8); }
static void p16(uint8_t *at, int16_t v) { at[0] = (uint8_t)v, at[1] = (uint8_t)((uint16_t)v >> 8); }

static int32_t g32(const uint8_t *at)
{
    return (int32_t)((uint32_t)at[0] | (uint32_t)at[1] << 8 | (uint32_t)at[2] << 16 | (uint32_t)at[3] << 24);
}

static void p32(uint8_t *at, int32_t v) { memcpy(at, &(uint32_t){ (uint32_t)v }, 4); }

static int type_of(const MenuRaceState *st) { return g16(st->image + RACE_TYPE); }
static int track_of(const MenuRaceState *st) { return g16(st->image + TRACK); }
static uint8_t *player_of(const MenuRaceState *st) { return st->game + PLAYER * *st->player; }

/* the record run for the track (a circuit: the difficulty's) */
static uint8_t *ghost_at(const MenuRaceState *st, int circuit)
{
    const int at = GHOST + TRACK_RECORDS * track_of(st) + (circuit ? GHOST_STRIDE * g16(st->image + DIFFICULTY) : 0);
    return st->image + at;
}

static void run_race(const MenuRaceState *st, char *const *argv, const uint8_t *black, const MenuRaceOps *ops)
{
    const int circuit = g16(st->game + TRACK_KEPT) > 2;
    if (circuit) snprintf(argv[1], ARG, "%s", STEM[g16(st->game + TRACK_KEPT)]);
    else snprintf(argv[1], ARG, "%s%d", STEM[g16(st->game + TRACK_KEPT)], g16(st->game + RUN) + 1);
    const uint8_t *ghost = ghost_at(st, circuit);
    for (int i = 0; i < MENU_RACE_GHOST; i++) st->ghost[i] = g32(ghost + 4 * i);
    if (*st->pointer_active) ops->pointer_hide(ops->ctx);
    ops->wait_frame(ops->ctx);
    ops->clip(ops->ctx, 0x20, 200, 0xc0, 0x120);
    ops->font_race(ops->ctx);
    ops->screen_mode(ops->ctx, 0);
    ops->race(ops->ctx, 2, argv);
    ops->font_set(ops->ctx, *st->font);
    ops->screen_mode(ops->ctx, 0);
    ops->present(ops->ctx);
    ops->palette_write(ops->ctx, 0, 0x100, black);
    ops->wait_frame(ops->ctx);
    ops->clip(ops->ctx, 0, 0, 0x280, 0x1e0);
    ops->origin(ops->ctx, 0x140, 0xf0);
    ops->screen_size(ops->ctx, 0x280, 0x1e0, 8);
    ops->present(ops->ctx);
    ops->palette_write(ops->ctx, 0, 0x100, black);
    ops->wait_frame(ops->ctx);
    ops->screen_mode(ops->ctx, 0);
    ops->pointer_reset(ops->ctx, 0);
    if (*st->pointer_active) ops->pointer_draw(ops->ctx);
}

/* the run's times into the player's record and 004fa0cc / 004fa0d8 */
static void keep_times(const MenuRaceState *st)
{
    uint8_t *const me = player_of(st);
    const int16_t run = (int16_t)(g16(st->game + RUN) + 1);
    p16(st->game + RUN, run);
    if (run >= 0 && run <= 3) {  /* port guard: the original writes 004fa0cc[run] and 004fa0d8[run] unchecked */
        st->times[3 + run] = g32(me + TOTAL);
        st->times[run] = g32(me + SPEED);
    }
    const int32_t lap = g32(me + LAP);
    if (track_of(st) > 2) p32(me + TRACK_TOTAL, lap > 0 ? (int32_t)((uint32_t)g16(st->image + LAPS) * (uint32_t)lap) : 0);
    else p32(me + TRACK_TOTAL, lap > 0 ? (int32_t)((uint32_t)g32(me + TRACK_TOTAL) + (uint32_t)lap) : 0);
    if (g32(me + TOP) < g32(me + SPEED)) p32(me + TOP, g32(me + SPEED));
}

/* a finished track that beat the best keeps its run */
static void keep_record(const MenuRaceState *st, const MenuRaceOps *ops)
{
    const int difficulty = track_of(st) > 2 ? g16(st->image + DIFFICULTY) : 0;
    const uint8_t *best = st->image + BEST + TRACK_RECORDS * track_of(st) + BEST_STRIDE * difficulty;
    if (g32(best) > g32(player_of(st) + TOTAL)) {
        uint8_t *const run = st->image + GHOST + TRACK_RECORDS * track_of(st) + GHOST_STRIDE * difficulty;
        for (int i = 0; i < MENU_RACE_GHOST; i++) p32(run + 4 * i, st->ghost[i]);
    }
    p16(st->image + SEGMENT, 0);
    ops->records_save(ops->ctx);
}

/* a finished race's screens; 1 when the race from the menus is over */
static int finished_screens(const MenuRaceState *st, const MenuRaceOps *ops)
{
    const int link = *st->link != 0;
    if (track_of(st) > 2) {
        if (!link || type_of(st) != CUP) {
            if (!link || type_of(st) != TOURNAMENT) ops->circuit_results(ops->ctx, 1);
        }
        if (type_of(st) == TOURNAMENT || type_of(st) == CUP) ops->standings(ops->ctx, 1);
        if (type_of(st) == TOURNAMENT) ops->tournament_points(ops->ctx), ops->standings(ops->ctx, 0);
        ops->after_race(ops->ctx);
        return 1;
    }
    if (g16(st->game + RUN) > 2) {
        const int type = type_of(st);
        if (!link || (type != TOURNAMENT && type != CUP)) ops->segment_results(ops->ctx, 1);
        if (type == TOURNAMENT || type == CUP) ops->standings(ops->ctx, 1);
        if (!link || (type != TOURNAMENT && type != CUP)) ops->track_results(ops->ctx, 1);
        if (type == TOURNAMENT) ops->tournament_points(ops->ctx), ops->standings(ops->ctx, 0);
        ops->after_race(ops->ctx);
        return 1;
    }
    const int type = type_of(st);
    if (!link || (type != TOURNAMENT && type != CUP)) ops->segment_results(ops->ctx, 1);
    if (type == TOURNAMENT || type == CUP) ops->standings(ops->ctx, 1);
    if ((int16_t)ops->next_segment(ops->ctx) != 0) return 0;
    p16(st->image + SEGMENT, 0);
    return 1;
}

/* the screens after a race given up or lost */
static void stopped_screens(const MenuRaceState *st, const MenuRaceOps *ops)
{
    const int type = type_of(st);
    if (track_of(st) > 2) {
        ops->circuit_results(ops->ctx, 0);
        if (type == TOURNAMENT || type == CUP) ops->standings(ops->ctx, 1);
        if (type == TOURNAMENT) ops->standings(ops->ctx, 0);
    } else if (g16(st->game + RUN) > 2) {
        ops->segment_results(ops->ctx, 1);
        if (type == TOURNAMENT || type == CUP) ops->standings(ops->ctx, 1);
        ops->track_results(ops->ctx, 0);
        if (type == TOURNAMENT) ops->standings(ops->ctx, 0);
    } else {
        ops->segment_results(ops->ctx, 1);
        if (type == TOURNAMENT || type == CUP) ops->standings(ops->ctx, 1);
    }
    ops->after_race(ops->ctx);
    p16(st->image + SEGMENT, 0);
}

/* the tournament's end: the tracks won counted; all nine or eight unlock the rest */
static void tournament_end(const MenuRaceState *st, const MenuRaceOps *ops)
{
    ops->tournament_reset(ops->ctx);
    int won = 0;
    for (int i = 0; i < TRACKS; i++) won += g16(st->game + WON + 2 * i);
    if (*st->link) {
        if (won == 9) {
            *st->warrior = 1, *st->unlock_m = 1, *st->unlock_r = 1;
            ops->game_reset(ops->ctx);
        } else if (won == 8) {
            *st->lost_vegas = 1;
            p16(st->image + TRACK, 8);
        }
        if (*st->host != 0) *st->link_done = 1;
        return;
    }
    if (*st->lost_vegas ? won != 9 : won != 8) return;
    if (ops->champion(ops->ctx, 0) != 0) {
        if (*st->lost_vegas) p16(st->image + LEVEL, 2);
        else if (g16(st->image + LEVEL) < 2) p16(st->image + LEVEL, 1);
        ops->unlocks(ops->ctx);
    }
    ops->game_reset(ops->ctx);
}

/* 00427ae0 */
void menu_race(const MenuRaceState *st, const MenuRaceOps *ops)
{
    char text[ARGS][ARG];
    char *argv[ARGS];
    uint8_t black[PALETTE_PUT_BYTES];
    *st->net_off = 1;
    ops->screen_mode(ops->ctx, 0);
    palette_put_clear(black);
    for (int i = 0; i < ARGS; i++) argv[i] = text[i];
    snprintf(argv[0], ARG, "nfs");
    for (int i = 1; i < MENU_RACE_TIMES; i++) st->times[i] = 0;
    p16(st->game + TYPE_KEPT, g16(st->image + RACE_TYPE));
    for (int done = 0; !done;) {
        memcpy(st->game + SETUP_KEPT, st->image + SETUP, SETUP_SIZE);
        memcpy(st->game + CONTROLS_KEPT, st->image + CONTROLS, CONTROLS_SIZE);
        run_race(st, argv, black, ops);
        if (*st->link_lost) {
            *st->net_off = 0;
            ops->music_pick(ops->ctx);
            return;
        }
        keep_times(st);
        memcpy(st->image + SETUP, st->game + SETUP_KEPT, SETUP_SIZE);
        memcpy(st->image + CONTROLS, st->game + CONTROLS_KEPT, CONTROLS_SIZE);
        if (g16(st->game + RACED) == ENDED_4) ops->unknown_428610(ops->ctx);
        if (g16(st->game + RACED) == FINISHED) {
            if (track_of(st) > 2 || g16(st->game + RUN) > 2) keep_record(st, ops);
            ops->bank_load(ops->ctx);
            ops->screen_mode(ops->ctx, 0);
            ops->music_pick(ops->ctx);
            *st->screen = ops->picture_new(ops->ctx, 0x280, 0x1e0, 0);
            done = finished_screens(st, ops);
        } else {
            ops->music_pick(ops->ctx);
            ops->bank_load(ops->ctx);
            *st->screen = ops->picture_new(ops->ctx, 0x280, 0x1e0, 0);
            stopped_screens(st, ops);
            done = 1;
        }
    }
    *st->net_off = 0;
    if (type_of(st) == TOURNAMENT) tournament_end(st, ops);
    else ops->game_reset(ops->ctx);
}
