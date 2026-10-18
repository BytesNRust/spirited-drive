#include "game/frontend/win_movie.h"

#include <stdio.h>

enum { WIDE = 0x280, HIGH = 0x1e0, WINS_AT = 0x580, RACES_AT = 0x592, TOTAL_AT = 0x65, MOVIE_ON_AT = 0x5b8,
       TRACKS = 9 };

static int16_t g16(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
static int32_t g32(const uint8_t *p) { return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24); }
static void p16(uint8_t *p, int16_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)((uint16_t)v >> 8); }

static int won(const WinMovieState *st)
{
    const int32_t mine = g32(st->game_state + GAME_STATE_PLAYER * st->player + TOTAL_AT);
    if (mine <= 0 || st->view != 0 || st->dropped[st->player] != 0) return 0;
    int w = 1;
    for (int32_t i = 0; i < st->racers; i++)
        if (i != st->player && mine > g32(st->game_state + GAME_STATE_PLAYER * i + TOTAL_AT)) w = 0;
    return w;
}

/* the last of type 3's: win#7, or winner.qfs while 0052b2fc is 0 */
static void last_win(const WinMovieState *st, const CopStopOps *ops)
{
    if (g16(st->game_state + MOVIE_ON_AT) != 0) {
        ops->movie(ops->ctx, "win#7.tgv");
        return;
    }
    *st->screen = ops->picture_new(ops->ctx, WIDE, HIGH, 0);
    ops->screen_show(ops->ctx, "winner.qfs", 0, 0);
    ops->picture_free(ops->ctx, *st->screen);
}

static const char *track_movie(uint16_t track)
{
    static const char *const by_track[8] = { "win#2.tgv", "win#4.tgv", "win#6.tgv", "win#1.tgv",
                                             "win#3.tgv", "win#3.tgv", "win#5.tgv", "win#5.tgv" };
    return track < 8 ? by_track[track] : "win#5.tgv";
}

/* 004283c4 */
void win_movie(const WinMovieState *st, const CopStopOps *ops)
{
    const int w = won(st);
    uint8_t *const at = st->game_state + 2 * st->track;
    p16(at + RACES_AT, 1);
    if (!w) return;
    if (st->race_type == 3) {
        p16(at + WINS_AT, 1);
        int32_t wins = 0;
        for (int i = 0; i < TRACKS - 1; i++) wins += g16(st->game_state + WINS_AT + 2 * i);
        const int32_t all = wins + g16(st->game_state + WINS_AT + 2 * (TRACKS - 1));
        if ((all == 9 && st->lost_vegas != 0) || (wins == 8 && st->lost_vegas == 0)) last_win(st, ops);
        else ops->movie(ops->ctx, track_movie((uint16_t)st->track));
        return;
    }
    if (st->race_type == 0) return;
    int16_t pick;
    do pick = (int16_t)ops->random_pick(ops->ctx);
    while (pick == 2);
    char name[0x50];
    snprintf(name, sizeof name, "win#%d.tgv", pick + (st->level < 3 ? 1 : st->level < 6 ? 3 : 5));
    ops->movie(ops->ctx, name);
}
