/* The port's intro option (extras.cfg's no_movies, intro_glue.c) skips the intro movies only: the victory movies
 * (win_movie, 004419c0) and the showroom's car video (showcase.c) still play while the game offers them. */
#include "game/frontend/intro.h"
#include "test.h"

#include <string.h>

static int played_count;
static char played[INTRO_PATH];

static void nothing(void *ctx) { (void)ctx; }
static void window(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h) { (void)ctx, (void)x, (void)y, (void)w, (void)h; }
static void palette_set(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx, (void)first, (void)count, (void)rgb; }
static void mode_set(void *ctx, int32_t w, int32_t h, int32_t bits) { (void)ctx, (void)w, (void)h, (void)bits; }
static int32_t no_file(void *ctx, const char *path) { (void)ctx, (void)path; return 0; }
static int32_t screen(void *ctx, const char *name, int32_t mode, int32_t music) { (void)ctx, (void)name, (void)mode, (void)music; return 0; }

static int32_t movie(void *ctx, const char *path)
{
    (void)ctx;
    played_count++;
    strncpy(played, path, sizeof played - 1);
    return 0;
}

static IntroOps ops_with(int32_t (*screen_show)(void *, const char *, int32_t, int32_t))
{
    const IntroOps ops = { nothing, nothing, window, no_file, NULL, nothing, palette_set, nothing, mode_set, movie, NULL,
                           screen_show, NULL, NULL };
    return ops;
}

/* the game's words for an intro with the movies offered (0052b2fc set) and the port's intro option off */
static IntroState intro_state(char *path)
{
    static int16_t replay_seen, replay, attract_seen, attract, replay_flag;
    static char replay_name[INTRO_REPLAY_NAME];
    replay_seen = 1;
    return (IntroState){ .intro_movies = 1, .replay_seen = &replay_seen, .replay = &replay, .attract_seen = &attract_seen,
                         .attract = &attract, .replay_flag = &replay_flag, .replay_name = replay_name,
                         .movie_dir = "MOVIES/", .movie_path = path, .intro_skip = 1 };
}

static void test_victory_movie_plays_with_the_intro_option_off(void)
{
    const IntroOps ops = ops_with(screen);
    char path[INTRO_PATH];
    IntroState s = intro_state(path);
    played_count = 0;
    CHECK_EQ_INT(intro_movie(&s, &ops, "win#2.tgv"), 1);
    CHECK_EQ_INT(played_count, 1);
    CHECK(strcmp(played, "MOVIES/win#2.tgv") == 0);
}

static void test_intro_movies_are_skipped_with_the_intro_option_off(void)
{
    const IntroOps ops = ops_with(screen);
    char path[INTRO_PATH];
    IntroState s = intro_state(path);
    played_count = 0;
    CHECK_EQ_INT(intro_show(&s, &ops), 1);
    CHECK_EQ_INT(played_count, 0);
}

static int32_t shown[1];
static int32_t screen_shown(void *ctx, const char *name, int32_t mode, int32_t music)
{
    (void)ctx, (void)mode, (void)music;
    shown[0] = strcmp(name, "eatitle.qfs") == 0;
    return 0;
}

static void test_the_intro_option_off_shows_the_title_screen(void)
{
    const IntroOps ops = ops_with(screen_shown);
    char path[INTRO_PATH];
    IntroState s = intro_state(path);
    shown[0] = 0;
    CHECK_EQ_INT(intro_show(&s, &ops), 1);
    CHECK_EQ_INT(shown[0], 1);
}

int main(void)
{
    test_victory_movie_plays_with_the_intro_option_off();
    test_intro_movies_are_skipped_with_the_intro_option_off();
    test_the_intro_option_off_shows_the_title_screen();
    return TEST_REPORT();
}
