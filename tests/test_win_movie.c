#include "tests/test.h"

#include "game/frontend/win_movie.h"

#include <string.h>

static char played[0x50];
static int plays;

static void movie(void *ctx, const char *file) { (void)ctx; snprintf(played, sizeof played, "%s", file), plays++; }
static int32_t pick(void *ctx) { (void)ctx; return 0; }
static int32_t picture_new(void *ctx, int32_t w, int32_t h, int32_t flags) { (void)ctx, (void)w, (void)h, (void)flags; return 1; }
static void screen_show(void *ctx, const char *file, int32_t mode, int32_t one) { (void)ctx, (void)file, (void)mode, (void)one; }
static void picture_free(void *ctx, int32_t picture) { (void)ctx, (void)picture; }

/* 004283c4: the player with the lowest total won, unless 0052b316 (only watching) is set */
static void run(int16_t view, int expected)
{
    uint8_t game[GAME_STATE_SIZE] = { 0 };
    const int16_t dropped[8] = { 0 };
    int32_t screen = 0;
    const int32_t total = 5, other = 9;
    memcpy(game + 0x65, &total, sizeof total);  /* player 0 has the lowest total (the best), player 1 a higher one */
    memcpy(game + 0x95 + 0x65, &other, sizeof other);
    const WinMovieState st = { game, 0, 2, view, dropped, 1, 3, 0, 0, &screen };
    const CopStopOps ops = { picture_new, screen_show, picture_free, pick, movie, NULL };
    plays = 0, played[0] = 0;
    win_movie(&st, &ops);
    CHECK_EQ_INT(plays, expected);
    if (expected) CHECK(strcmp(played, "win#1.tgv") == 0);
    CHECK_EQ_INT(game[0x592 + 2 * 3], 1);  /* the track's race is marked run either way */
}

/* a track word past the nine (from a host on the network): no word of the game state is written, the movie win#5 */
static void unknown_track(int16_t track)
{
    uint8_t game[GAME_STATE_SIZE] = { 0 }, before[GAME_STATE_SIZE];
    const int16_t dropped[8] = { 0 };
    int32_t screen = 0;
    const int32_t total = 5, other = 9;
    memcpy(game + 0x65, &total, sizeof total);
    memcpy(game + 0x95 + 0x65, &other, sizeof other);
    memcpy(before, game, sizeof game);
    const WinMovieState st = { game, 0, 2, 0, dropped, 3, track, 0, 0, &screen };
    const CopStopOps ops = { picture_new, screen_show, picture_free, pick, movie, NULL };
    plays = 0, played[0] = 0;
    win_movie(&st, &ops);
    CHECK(memcmp(game, before, sizeof game) == 0);
    CHECK_EQ_INT(plays, 1);
    CHECK(strcmp(played, "win#5.tgv") == 0);
}

int main(void)
{
    run(0, 1);
    run(1, 0);
    unknown_track(9);
    unknown_track(15);
    unknown_track(-1);   /* 0xFFFF */
    return TEST_REPORT();
}
