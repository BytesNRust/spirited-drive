/* port (owner 2026-11-24): the track screen writes the picked variant's name (Mirror, Rally) only in Restomod; Stock and Restored show the original's screen. The screen's own fakes
 * are tests/test_track_select_screen.c's (included here, its main renamed); the profile is chosen with the real settings under a scratch player folder. */
#define main track_select_screen_main
#include "test_track_select_screen.c"
#undef main

#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static int words_in(Profile p, int mirror, int rally)
{
    CHECK(port_profile_select(p));
    variant_m_in = mirror, variant_r_in = rally;
    switches = 0;
    variant_words = 0;
    int weather = 0, light_time = 0;
    run(NULL, 0, &weather, &light_time, 0);   /* the screen drawn once, Escape */
    variant_m_in = variant_r_in = 0;
    return variant_words;
}

static void test_the_variant_name_is_restomods(void)
{
    CHECK(words_in(PROFILE_RESTOMOD, 1, 0) > 0);   /* Mirror */
    CHECK(words_in(PROFILE_RESTOMOD, 0, 1) > 0);   /* Rally */
    CHECK_EQ_INT(words_in(PROFILE_RESTOMOD, 0, 0), 0);   /* no variant picked: nothing */
    const Profile others[] = { PROFILE_STOCK, PROFILE_RESTORED };
    for (int i = 0; i < 2; i++) {
        CHECK_EQ_INT(words_in(others[i], 1, 0), 0);
        CHECK_EQ_INT(words_in(others[i], 0, 1), 0);
    }
}

int main(void)
{
    char base[256], pref[300];
    snprintf(base, sizeof base, "%s/tnfs_variant_name_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(pref, sizeof pref, "%s/pref", base);
    mkdir(base, 0777), mkdir(pref, 0777);
    setenv("TNFS_PREF_DIR", pref, 1);
    test_the_variant_name_is_restomods();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
