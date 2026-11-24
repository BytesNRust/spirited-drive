/* port (owner 2026-11-23): the lost-link banner says "The host's track is not in this version" only in Restomod; Stock and Restored keep the original's "Connection Terminated"
 * (Verbindung abgebrochen), also after the game was left for the host's track. A plain lost link is the original's text in every mode. */
#include "game/frontend/link_central.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *const ENGLISH = "Connection Terminated", *const GERMAN = "Verbindung abgebrochen";

static void test_the_banner_by_mode(void)
{
    const Profile modes[] = { PROFILE_STOCK, PROFILE_RESTORED, PROFILE_RESTOMOD };
    for (int i = 0; i < 3; i++) {
        CHECK(port_profile_select(modes[i]));
        link_central_left_forget();
        CHECK(strcmp(link_central_lost_text(ENGLISH, 0), ENGLISH) == 0);   /* a lost link: the original's, in every mode */
        CHECK(strcmp(link_central_lost_text(GERMAN, 1), GERMAN) == 0);
        link_central_host_track_refused();
        if (modes[i] == PROFILE_RESTOMOD) {
            CHECK(strcmp(link_central_lost_text(ENGLISH, 0), "The host's track is not in this version") == 0);
            CHECK(strcmp(link_central_lost_text(ENGLISH, 0), ENGLISH) == 0);   /* told once */
        } else {
            CHECK(strcmp(link_central_lost_text(ENGLISH, 0), ENGLISH) == 0);   /* the original's, though the game was left for the host's track */
            link_central_host_track_refused();
            CHECK(strcmp(link_central_lost_text(GERMAN, 1), GERMAN) == 0);
        }
    }
    link_central_host_track_refused();
    CHECK(port_profile_select(PROFILE_RESTOMOD));
    CHECK(strcmp(link_central_lost_text(GERMAN, 1), "Strecke des Hosts unbekannt") == 0);
}

int main(void)
{
    char base[256], pref[300];
    snprintf(base, sizeof base, "%s/tnfs_link_banner_test_%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
    snprintf(pref, sizeof pref, "%s/pref", base);
    mkdir(base, 0777), mkdir(pref, 0777);
    setenv("TNFS_PREF_DIR", pref, 1);
    test_the_banner_by_mode();
    char cmd[400];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", base);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", base);
    return TEST_REPORT();
}
