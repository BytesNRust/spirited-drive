/* Where Studio looks for the game (editor/game_find.h): every place in its order on a Mac laid out by the test, from a working directory of / (the
 * finder never asks the file system but through is_file). */
#include "game_find.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static const char *files[8];
static int nfiles;
static int bundle_calls;
static const char *bundle_app;

static int is_file(const char *path, void *ctx)
{
    (void)ctx;
    for (int i = 0; i < nfiles; i++)
        if (strcmp(files[i], path) == 0) return 1;
    return 0;
}

static int by_bundle(char *app, size_t cap, void *ctx)
{
    (void)ctx;
    bundle_calls++;
    if (bundle_app == NULL) return 0;
    snprintf(app, cap, "%s", bundle_app);
    return 1;
}

static void lay(const char *a, const char *b)
{
    nfiles = 0;
    if (a != NULL) files[nfiles++] = a;
    if (b != NULL) files[nfiles++] = b;
    bundle_calls = 0;
    bundle_app = NULL;
}

#define STUDIO_BASE "/Applications/Spirited Drive Studio.app/Contents/Resources/"
#define SIBLING "/Applications/Spirited Drive Studio.app/Contents/Resources/../../../Spirited Drive.app/Contents/MacOS/tnfs"

static GameFinder finder(const char *base, const char *home, const char *remembered)
{
    GameFinder f = { base, home, remembered, is_file, by_bundle, NULL };
    return f;
}

int main(void)
{
    char out[GAME_FIND_PATH], tried[GAME_FIND_TRIED];

    /* the app beside Studio, wherever both were dragged: the first place after a remembered one */
    lay("/Volumes/Games/Apps/Spirited Drive Studio.app/Contents/Resources/../../../Spirited Drive.app/Contents/MacOS/tnfs", NULL);
    GameFinder f = finder("/Volumes/Games/Apps/Spirited Drive Studio.app/Contents/Resources/", "/home/p/", NULL);
    CHECK_EQ_INT(game_find(&f, out, sizeof out, tried, sizeof tried), 1);
    CHECK(strstr(out, "Games/Apps/Spirited Drive Studio.app/Contents/Resources/../../../Spirited Drive.app/Contents/MacOS/tnfs") != NULL);
    CHECK_EQ_INT(bundle_calls, 0);   /* nothing else was asked once it was found */

    /* Applications, with Studio elsewhere (a copy run from the Downloads folder, a build tree) */
    lay("/Applications/Spirited Drive.app/Contents/MacOS/tnfs", NULL);
    f = finder("/home/p/Downloads/Spirited Drive Studio.app/Contents/Resources/", "/home/p/", NULL);
    CHECK_EQ_INT(game_find(&f, out, sizeof out, tried, sizeof tried), 1);
    CHECK(strcmp(out, "/Applications/Spirited Drive.app/Contents/MacOS/tnfs") == 0);

    /* App Translocation hides the neighbour: its path is not tried at all, /Applications is */
    lay(SIBLING, "/Applications/Spirited Drive.app/Contents/MacOS/tnfs");
    f = finder("/private/var/folders/ab/xyz/AppTranslocation/1234/d/Spirited Drive Studio.app/Contents/Resources/", "/home/p/", NULL);
    CHECK_EQ_INT(game_find(&f, out, sizeof out, tried, sizeof tried), 1);
    CHECK(strcmp(out, "/Applications/Spirited Drive.app/Contents/MacOS/tnfs") == 0);
    CHECK(strstr(tried, "AppTranslocation") == NULL);

    /* ~/Applications */
    lay("/home/p/Applications/Spirited Drive.app/Contents/MacOS/tnfs", NULL);
    f = finder(STUDIO_BASE, "/home/p/", NULL);
    CHECK_EQ_INT(game_find(&f, out, sizeof out, tried, sizeof tried), 1);
    CHECK(strcmp(out, "/home/p/Applications/Spirited Drive.app/Contents/MacOS/tnfs") == 0);

    /* the app the system knows by its bundle id (renamed, or on another volume) */
    lay("/Volumes/Big/Spirited Drive 2.app/Contents/MacOS/tnfs", NULL);
    bundle_app = "/Volumes/Big/Spirited Drive 2.app";
    f = finder(STUDIO_BASE, "/home/p/", NULL);
    CHECK_EQ_INT(game_find(&f, out, sizeof out, tried, sizeof tried), 1);
    CHECK(strcmp(out, "/Volumes/Big/Spirited Drive 2.app/Contents/MacOS/tnfs") == 0);
    CHECK_EQ_INT(bundle_calls, 1);

    /* the program the user located wins over every guess */
    lay("/Applications/Spirited Drive.app/Contents/MacOS/tnfs", "/Volumes/Big/Mine.app/Contents/MacOS/tnfs");
    f = finder(STUDIO_BASE, "/home/p/", "/Volumes/Big/Mine.app/Contents/MacOS/tnfs");
    CHECK_EQ_INT(game_find(&f, out, sizeof out, tried, sizeof tried), 1);
    CHECK(strcmp(out, "/Volumes/Big/Mine.app/Contents/MacOS/tnfs") == 0);
    f = finder(STUDIO_BASE, "/home/p/", "/gone/Mine.app/Contents/MacOS/tnfs");   /* a remembered one that is gone: the others still work */
    CHECK_EQ_INT(game_find(&f, out, sizeof out, tried, sizeof tried), 1);
    CHECK(strcmp(out, "/Applications/Spirited Drive.app/Contents/MacOS/tnfs") == 0);

    /* a build tree: ./tnfs */
    lay("./tnfs", NULL);
    f = finder(NULL, NULL, NULL);
    CHECK_EQ_INT(game_find(&f, out, sizeof out, tried, sizeof tried), 1);
    CHECK(strcmp(out, "./tnfs") == 0);

    /* nothing: ./tnfs as the answer, 0, and every place looked at is told */
    lay(NULL, NULL);
    f = finder(STUDIO_BASE, "/home/p/", "/gone/Mine.app/Contents/MacOS/tnfs");
    CHECK_EQ_INT(game_find(&f, out, sizeof out, tried, sizeof tried), 0);
    CHECK(strcmp(out, "./tnfs") == 0);
    CHECK(strstr(tried, "/gone/Mine.app") != NULL && strstr(tried, "../../../Spirited Drive.app/Contents/MacOS/tnfs") != NULL);
    CHECK(strstr(tried, "/Applications/Spirited Drive.app/Contents/MacOS/tnfs") != NULL && strstr(tried, "/home/p/Applications/") != NULL);
    CHECK(strstr(tried, "./tnfs") != NULL);

    /* what Locate game hands back: an app folder turns into its program */
    CHECK_EQ_INT(game_find_program_of("/Applications/Spirited Drive.app", out, sizeof out), 1);
    CHECK(strcmp(out, "/Applications/Spirited Drive.app/Contents/MacOS/tnfs") == 0);
    CHECK_EQ_INT(game_find_program_of("/Applications/Spirited Drive.app/", out, sizeof out), 1);
    CHECK(strcmp(out, "/Applications/Spirited Drive.app/Contents/MacOS/tnfs") == 0);
    CHECK_EQ_INT(game_find_program_of("/x/Spirited Drive.app/Contents/MacOS/tnfs", out, sizeof out), 1);
    CHECK(strcmp(out, "/x/Spirited Drive.app/Contents/MacOS/tnfs") == 0);
    CHECK_EQ_INT(game_find_program_of("/Applications/Notes.txt", out, sizeof out), 0);
    CHECK_EQ_INT(game_find_program_of("", out, sizeof out), 0);
    CHECK_EQ_INT(game_find_program_of(NULL, out, sizeof out), 0);
    CHECK_EQ_INT(game_find_is_app_program("/a/b.app/Contents/MacOS/tnfs"), 1);
    CHECK_EQ_INT(game_find_is_app_program("./tnfs"), 0);
    return TEST_REPORT();
}
