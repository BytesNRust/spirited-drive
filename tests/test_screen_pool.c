/* The screen picture's pool across races: each race's end (cop_stop, win_movie) and each race's start must return the
 * picture it had, or the 16 picture slots of ea_lib fill up after a few races and the in-race Yes / No box gets 0 for its
 * pictures. */
#include "tests/test.h"

#include "formats/fsh.h"
#include "game/frontend/cop_stop.h"
#include "game/frontend/ea_lib.h"
#include "game/frontend/tournament.h"
#include "game/frontend/win_movie.h"

#include <stdbool.h>
#include <string.h>

enum { POOL_SLOTS = 16, RACES = 40 };  /* ea_lib.c's PICTURES */

/* the video and pointer calls ea_lib makes: the picture pool needs none of them */
void video_select_target(uint8_t *pixels, int w, int h) { (void)pixels, (void)w, (void)h; }
uint8_t *video_target(int *w, int *h) { static uint8_t px[1]; *w = *h = 0; return px; }
void video_get_palette(uint8_t rgb[768]) { memset(rgb, 0, 768); }
void video_read_rect(int x, int y, int w, int h, uint8_t *out) { (void)x, (void)y, (void)w, (void)h, (void)out; }
void video_blit(const FshEntry *shape, int x, int y) { (void)shape, (void)x, (void)y; }
void video_blit_at_own_position(const FshEntry *shape) { (void)shape; }
void video_blit_transparent(const FshEntry *shape, int x, int y) { (void)shape, (void)x, (void)y; }
void video_blit_transparent_at_own_position(const FshEntry *shape) { (void)shape; }
void pointer_range(int x0, int y0, int x1, int y1) { (void)x0, (void)y0, (void)x1, (void)y1; }
/* the platform's files: the resource loader is linked in for its banks, no file is read here */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path, (void)size; return NULL; }
void platform_free_file(uint8_t *data) { (void)data; }
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size) { (void)name, (void)data, (void)size; return false; }

static int32_t picture_new(void *ctx, int32_t w, int32_t h, int32_t flags) { (void)ctx, (void)flags; return ea_picture_new(w, h); }
static void picture_free(void *ctx, int32_t picture) { (void)ctx; ea_picture_free(picture); }
static void screen_show(void *ctx, const char *file, int32_t mode, int32_t one) { (void)ctx, (void)file, (void)mode, (void)one; }
static int32_t pick(void *ctx) { (void)ctx; return 0; }
static int plays;
static void movie(void *ctx, const char *file) { (void)ctx, (void)file, plays++; }

/* how many pictures the pool still gives out (all taken back before returning) */
static int free_slots(void)
{
    int32_t taken[POOL_SLOTS];
    int n = 0;
    while (n < POOL_SLOTS && (taken[n] = ea_picture_new(4, 4)) != 0) n++;
    for (int i = 0; i < n; i++) ea_picture_free(taken[i]);
    return n;
}

/* 00428610 with movies off, once per race: the race's screen is there when the cops end it (the race's menu made it) */
static void cop_stop_races(void)
{
    const int16_t movie_on = 0, german = 0;
    const CopStopOps ops = { picture_new, screen_show, picture_free, pick, movie, NULL };
    for (int race = 0; race < RACES; race++) {
        int32_t *const screen = ea_screen_slot();
        CHECK(ea_screen() != 0);  /* the race left its screen in the slot */
        const CopStopState st = { &movie_on, &german, screen };
        cop_stop(&st, &ops);
        CHECK(*screen == 0);  /* the ticket's picture is freed after the ticket is shown */
    }
}

/* 004283c4's last win (all nine tracks won, movies off), once per race */
static void win_movie_races(void)
{
    enum { WINS_AT = 0x580, TOTAL_AT = 0x65, MOVIE_ON_AT = 0x5b8 };
    uint8_t game[GAME_STATE_SIZE] = { 0 };
    const int16_t dropped[8] = { 0 };
    const int32_t total = 5;
    memcpy(game + TOTAL_AT, &total, sizeof total);
    for (int i = 1; i < 8; i++) {
        const int16_t won = 1;
        memcpy(game + WINS_AT + 2 * i, &won, sizeof won);
    }
    const CopStopOps ops = { picture_new, screen_show, picture_free, pick, movie, NULL };
    for (int race = 0; race < RACES; race++) {
        int32_t *const screen = ea_screen_slot();
        CHECK(ea_screen() != 0);  /* the race left its screen in the slot */
        const WinMovieState st = { game, 0, 1, 0, dropped, 3, 0, 0, 0, screen };
        win_movie(&st, &ops);
        CHECK(*screen == 0);
    }
    CHECK_EQ_INT(game[MOVIE_ON_AT], 0);
}

/* race(): the screen Control Central left is freed and the race starts with none, once per race */
static void race_start_races(void)
{
    for (int race = 0; race < RACES; race++) {
        CHECK(ea_screen() != 0);  /* the race's menus and the pause make it when they need it */
        ea_screen_release();
    }
}

int main(void)
{
    cop_stop_races();
    CHECK_EQ_INT(free_slots(), POOL_SLOTS);
    win_movie_races();
    CHECK_EQ_INT(free_slots(), POOL_SLOTS);
    race_start_races();
    CHECK_EQ_INT(free_slots(), POOL_SLOTS);
    CHECK_EQ_INT(plays, 0);  /* the last win shows winner.qfs, not a movie */
    return TEST_REPORT();
}
