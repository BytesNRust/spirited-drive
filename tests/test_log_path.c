/* The paths the bug-report log carries have no player's name in them (log_path.h). */
#include "game/sys/log_path.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static int says(const char *path, const char *home, const char *user, const char *want)
{
    char out[200];
    log_path_for_report(path, home, user, out, sizeof out);
    if (strcmp(out, want) != 0) fprintf(stderr, "path %s -> %s, wanted %s\n", path, out, want);
    return strcmp(out, want) == 0;
}

int main(void)
{
    const char *home = "/hm/pat", *user = "/hm/pat/Library/Application Support/Spirited Drive/";
    CHECK(says("/hm/pat/Library/Application Support/Spirited Drive/gamedata", home, user, "<user>/gamedata"));   /* the player's folder first (its trailing slash does not matter) */
    CHECK(says("/hm/pat/Games/NFS", home, user, "~/Games/NFS"));
    CHECK(says("/Volumes/NFS_CD/", home, user, "/Volumes/NFS_CD/"));                                              /* another place: as it is */
    CHECK(says("/hm/patty/Games", home, user, "/hm/patty/Games"));                                          /* whole names only */
    CHECK(says("/hm/pat", home, user, "~"));
    CHECK(says("C:\\Home\\Pat\\AppData\\Roaming\\Spirited Drive\\gamedata", "C:\\Home\\Pat", "C:\\Home\\Pat\\AppData\\Roaming\\Spirited Drive\\", "<user>\\gamedata"));
    CHECK(says("D:\\NFS", "C:\\Home\\Pat", "C:\\Home\\Pat\\AppData\\Roaming\\Spirited Drive\\", "D:\\NFS"));
    CHECK(says("/hm/pat/x", NULL, NULL, "/hm/pat/x"));                                                       /* nothing to hide */
    CHECK(says("/hm/pat/x", "", "", "/hm/pat/x"));
    CHECK(says("", home, user, ""));
    char small[8];
    log_path_for_report("/hm/pat/Games/NFS", home, user, small, sizeof small);
    CHECK(strlen(small) < sizeof small);                                                                          /* cut, not overrun */
    return TEST_REPORT();
}
