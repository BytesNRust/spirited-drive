/* dep59 step 4b (T1): the files dropped on the window (platform.h's platform_dropped_file), oldest first, as the CONTENT screen installs
 * them: SDL's drop events, and the input script's ms:DROP:name (TNFS_DROP_DIR/name, a name of a-z 0-9 _ - . not starting with a dot: no
 * path) for the headless walks. The queue keeps PLATFORM_DROPPED_MAX; one more, or a path that does not fit, is left out with a line in
 * the log. platform_pick_zip with no window shows no dialog and picks nothing. SDL's dummy video driver: no window is shown. */
#include "platform/platform.h"
#include "test.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SDL's event for a file dropped on the window, as the system sends it */
static void drop(const char *path)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_DROP_FILE;
    e.drop.data = path;
    CHECK(SDL_PushEvent(&e));
}

/* the next path taken, "" when none */
static const char *next(void)
{
    static char out[1100];
    out[0] = '\0';
    (void)platform_poll_events();
    if (!platform_dropped_file(out, sizeof out)) out[0] = '\0';
    return out;
}

static void test_the_events_oldest_first(void)
{
    CHECK(strcmp(next(), "") == 0);   /* nothing dropped */
    drop("/tmp/one.zip"), drop("/tmp/two.zip");
    CHECK(strcmp(next(), "/tmp/one.zip") == 0);
    CHECK(strcmp(next(), "/tmp/two.zip") == 0);
    CHECK(strcmp(next(), "") == 0);
    static char paths[PLATFORM_DROPPED_MAX + 1][32];   /* alive until the poll reads the events, as SDL's own drop data is */
    for (int i = 0; i < PLATFORM_DROPPED_MAX + 1; i++) {   /* one more than it keeps: the last left out */
        snprintf(paths[i], sizeof paths[i], "/tmp/%d.zip", i);
        drop(paths[i]);
    }
    (void)platform_poll_events();
    char out[1100];
    int taken = 0;
    while (platform_dropped_file(out, sizeof out)) taken++;
    CHECK_EQ_INT(taken, PLATFORM_DROPPED_MAX);
    drop("/tmp/a-long-enough-name.zip");
    (void)platform_poll_events();
    char small[8];
    CHECK(!platform_dropped_file(small, sizeof small));   /* does not fit: left out, not cut */
    CHECK(!platform_dropped_file(out, sizeof out));
}

/* the script's DROP items: TNFS_DROP_DIR/name, in turn; a name with a path or a leading dot is a bad item, never queued */
static void test_the_script(void)
{
    char out[1100];
    int taken = 0, bad = 0;
    for (int i = 0; i < 300 && taken < 2; i++) {   /* up to 3 s */
        (void)platform_poll_events();
        if (!platform_dropped_file(out, sizeof out)) {
            SDL_Delay(10);
            continue;
        }
        if (taken == 0) CHECK(strcmp(out, "/drops/car.zip") == 0);
        if (taken == 1) CHECK(strcmp(out, "/drops/other_car-2.zip") == 0);
        if (strstr(out, "..") != NULL || strstr(out, "etc") != NULL) bad++;
        taken++;
    }
    CHECK_EQ_INT(taken, 2);
    CHECK_EQ_INT(bad, 0);
}

int main(void)
{
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    setenv("TNFS_DROP_DIR", "/drops", 1);
    setenv("TNFS_INPUT_SCRIPT", "1000:DROP:car.zip,1000:DROP:../etc,1000:DROP:.hidden,1000:DROP:a/b.zip,1100:DROP:other_car-2.zip", 1);
    unsetenv("TNFS_INPUT_SETTLE");
    PlatformConfig config = { 640, 480, "test", 0, 0, 0 };
    CHECK(platform_init(&config));
    test_the_events_oldest_first();   /* before the script's first drop is due (1 s; each item at its own ms with no TNFS_INPUT_SETTLE) */
    test_the_script();
    char picked[64] = "x";
    CHECK(!platform_pick_zip(picked, sizeof picked, NULL) && picked[0] == '\0');   /* no window (dummy video): no dialog, none picked */
    platform_shutdown();
    return TEST_REPORT();
}
