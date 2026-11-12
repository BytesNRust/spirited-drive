/* port: what the game shows first (the owner's order): our boot screen, then EA's movies (ea.tgv, an attract movie, title.tgv) or, with the
 * movies off, EA's title screen. The boot screen is shown on every path of the intro, so in every profile (Stock, Restored, Restomod: what they
 * change here is only whether the movies play), and only from the intro, which the game reaches after the first run's setup. Without the port's
 * boot op the intro is the exe's. */
#include "game/frontend/intro.h"
#include "test.h"

#include <string.h>

enum { EVENTS = 16, EVENT_CHARS = 32 };
static char events[EVENTS][EVENT_CHARS];
static int count;

static void log_event(const char *what)
{
    if (count < EVENTS) strncpy(events[count++], what, EVENT_CHARS - 1);
}

static void nothing(void *ctx) { (void)ctx; }
static void drain(void *ctx) { (void)ctx; log_event("drain"); }
static void boot(void *ctx) { (void)ctx; log_event("boot"); }
static void window(void *ctx, int32_t x, int32_t y, int32_t w, int32_t h) { (void)ctx, (void)x, (void)y, (void)w, (void)h; }
static void palette_set(void *ctx, int32_t first, int32_t count_, const uint8_t *rgb) { (void)ctx, (void)first, (void)count_, (void)rgb; }
static void mode_set(void *ctx, int32_t w, int32_t h, int32_t bits) { (void)ctx, (void)w, (void)h, (void)bits; }
static int32_t no_file(void *ctx, const char *path) { (void)ctx, (void)path; return 0; }
static int32_t pick(void *ctx) { (void)ctx; return 0; }
static int32_t movie(void *ctx, const char *path)
{
    (void)ctx;
    const char *slash = strrchr(path, '/');
    log_event(slash != NULL ? slash + 1 : path);
    return 0;
}
static int32_t screen(void *ctx, const char *name, int32_t mode, int32_t music)
{
    (void)ctx, (void)mode, (void)music;
    log_event(name);
    return 0;
}

static IntroOps ops_with(void (*boot_op)(void *))
{
    const IntroOps ops = { nothing, drain, window, no_file, NULL, nothing, palette_set, nothing, mode_set, movie, pick, screen, NULL, boot_op };
    return ops;
}

/* the game's words for its first intro (no replay yet), the movies offered by the game (0052b2fc) and on or off in the profile */
static IntroState state(char *path, int movies, int skip)
{
    static int16_t replay_seen, replay, attract_seen, attract, replay_flag;
    static char replay_name[INTRO_REPLAY_NAME];
    replay_seen = replay = attract_seen = attract = replay_flag = 0;
    return (IntroState){ .intro_movies = movies, .replay_seen = &replay_seen, .replay = &replay, .attract_seen = &attract_seen,
                         .attract = &attract, .replay_flag = &replay_flag, .replay_name = replay_name,
                         .movie_dir = "MOVIES/", .movie_path = path, .intro_skip = skip };
}

static void run(int movies, int skip, void (*boot_op)(void *))
{
    const IntroOps ops = ops_with(boot_op);
    char path[INTRO_PATH];
    IntroState s = state(path, movies, skip);
    count = 0;
    CHECK_EQ_INT(intro_show(&s, &ops), 1);
}

static int at(const char *what)
{
    for (int i = 0; i < count; i++)
        if (strcmp(events[i], what) == 0) return i;
    return -1;
}

static void the_boot_screen_comes_before_eas_movies(void)
{
    run(1, 0, boot);
    CHECK_EQ_INT(at("boot"), 0);
    CHECK(at("ea.tgv") > at("boot"));
    CHECK(at("attract1.tgv") > at("ea.tgv"));
    CHECK(at("title.tgv") > at("attract1.tgv"));
}

/* a key pressed on the boot screen is drained before EA's movies, so it does not stop the first of them */
static void its_keys_are_drained_before_the_movies(void)
{
    run(1, 0, boot);
    CHECK(at("drain") > at("boot"));
    CHECK(at("drain") < at("ea.tgv"));
}

/* a profile with the movies off: the boot screen, then EA's title screen */
static void with_the_movies_off_it_comes_before_eas_title_screen(void)
{
    run(1, 1, boot);
    CHECK_EQ_INT(at("boot"), 0);
    CHECK(at("eatitle.qfs") > at("boot"));
    run(0, 0, boot);
    CHECK_EQ_INT(at("boot"), 0);
    CHECK(at("eatitle.qfs") > at("boot"));
}

static void without_the_ports_op_the_intro_is_the_exes(void)
{
    run(1, 0, NULL);
    CHECK_EQ_INT(at("boot"), -1);
    CHECK_EQ_INT(at("drain"), 0);
    CHECK(at("ea.tgv") > 0);
}

/* the replay of the intro the menus ask for later shows only the movies: the boot screen is once a run (boot_screen_show), the op is asked each time */
static int boots;
static void boot_once(void *ctx)
{
    (void)ctx;
    if (boots++ == 0) log_event("boot");
}
static void a_second_intro_asks_again_and_the_screen_says_no(void)
{
    boots = 0;
    run(1, 0, boot_once);
    run(1, 0, boot_once);
    CHECK_EQ_INT(boots, 2);
    CHECK_EQ_INT(at("boot"), -1);
    CHECK(at("ea.tgv") >= 0);
}

int main(void)
{
    the_boot_screen_comes_before_eas_movies();
    its_keys_are_drained_before_the_movies();
    with_the_movies_off_it_comes_before_eas_title_screen();
    without_the_ports_op_the_intro_is_the_exes();
    a_second_intro_asks_again_and_the_screen_says_no();
    return TEST_REPORT();
}
