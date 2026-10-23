/* The intro's movie path (intro.h): a movie directory as long as a data folder anywhere on disk reaches the movie
 * whole; the exe's 0x50 bytes cut it, and the cut name was skipped as missing. */
#include "game/frontend/intro.h"
#include "test.h"

#include <string.h>

static char played[1024];

static void nothing(void *ctx) { (void)ctx; }
static void window(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h) { (void)ctx, (void)x, (void)y, (void)w, (void)h; }
static void palette_set(void *ctx, int32_t first, int32_t count, const uint8_t *rgb) { (void)ctx, (void)first, (void)count, (void)rgb; }
static void mode_set(void *ctx, int32_t w, int32_t h, int32_t bits) { (void)ctx, (void)w, (void)h, (void)bits; }

static int32_t movie(void *ctx, const char *path)
{
    (void)ctx;
    strncpy(played, path, sizeof played - 1);
    return 1;
}

static void test_a_long_movie_directory_reaches_the_movie(void)
{
    const IntroOps ops = { nothing, nothing, window, NULL, NULL, nothing, palette_set, nothing, mode_set, movie, NULL,
                           NULL, NULL };
    static const char DIR[] = "/Users/someone/Library/Application Support/Games/Need for Speed SE/original/FRONTEND/"
                              "MOVIES/";
    char path[INTRO_PATH];
    IntroState s = { .intro_movies = 1, .movie_dir = DIR, .movie_path = path };
    CHECK_EQ_INT(intro_movie(&s, &ops, "attract2.tgv"), 1);
    CHECK(strcmp(played, "/Users/someone/Library/Application Support/Games/Need for Speed SE/original/FRONTEND/"
                         "MOVIES/attract2.tgv") == 0);
}

int main(void)
{
    test_a_long_movie_directory_reaches_the_movie();
    return TEST_REPORT();
}
