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

static const char *home_pat(void) { return "/hm/pat"; }

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
#ifdef _WIN32   /* Windows paths differ in case alone: the drive letter, the folder names */
    CHECK(says("c:\\users\\pat\\x", "C:\\Home\\Pat", NULL, "c:\\users\\pat\\x"));
    CHECK(says("c:\\home\\pat\\x", "C:\\Home\\Pat", NULL, "~\\x"));
    CHECK(says("C:\\HOME\\PAT\\AppData", "c:\\home\\pat", "c:\\home\\pat\\appdata", "<user>"));
#else           /* elsewhere case matters */
    CHECK(says("/hm/PAT/x", home, NULL, "/hm/PAT/x"));
#endif
    /* free text: every place a folder is named, not only a path's start */
    char text[300];
    log_text_for_report("platform: cannot move /hm/pat/Games/a to /hm/pat/Games/b: no room", home_pat(), NULL, text, sizeof text);
    CHECK(strcmp(text, "platform: cannot move ~/Games/a to ~/Games/b: no room") == 0);
    log_text_for_report("user: 2 item(s) moved from /hm/pat/App/Spirited Drive/mods to /hm/pat/App/Spirited Drive/x", "/hm/pat", "/hm/pat/App/Spirited Drive/", text, sizeof text);
    CHECK(strcmp(text, "user: 2 item(s) moved from <user>/mods to <user>/x") == 0);   /* the player's folder before the home */
    log_text_for_report("mark:movie=/hm/pat/App/Spirited Drive/gamedata/x.tgv", "/hm/pat", "/hm/pat/App/Spirited Drive", text, sizeof text);
    CHECK(strcmp(text, "mark:movie=<user>/gamedata/x.tgv") == 0);
    log_text_for_report("in /data/hm/pat/x and /hm/patty/x and /hm/pat", "/hm/pat", NULL, text, sizeof text);
    CHECK(strcmp(text, "in /data/hm/pat/x and /hm/patty/x and ~") == 0);   /* a longer path or name is not it; the end of the text is */
    log_text_for_report("nothing to hide here", "/hm/pat", "/hm/pat/x", text, sizeof text);
    CHECK(strcmp(text, "nothing to hide here") == 0);
    log_text_for_report("/hm/pat/x", NULL, NULL, text, sizeof text);
    CHECK(strcmp(text, "/hm/pat/x") == 0);
    CHECK(log_text_for_report("a /hm/pat/b", "/hm/pat", NULL, text, 5) == 4);   /* cut to the room, NUL-terminated */
    char small[8];
    log_path_for_report("/hm/pat/Games/NFS", home, user, small, sizeof small);
    CHECK(strlen(small) < sizeof small);                                                                          /* cut, not overrun */
    return TEST_REPORT();
}
