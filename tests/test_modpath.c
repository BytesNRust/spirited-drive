/* Mod overlay path rules: what lies under the data root, which relative names are safe, and the real-path
 * containment test that stops a symbolic link from leading out of the mod directory. */
#include "formats/modpath.h"
#include "test.h"

#include <string.h>

static const char *rel_of(const char *root, const char *path, int *ok)
{
    static char rel[256];
    *ok = modpath_relative(root, path, rel, sizeof rel);
    return rel;
}

int main(void)
{
    int ok;
    CHECK(strcmp(rel_of("original", "original/SIMDATA/MISC/TR1.TRI", &ok), "SIMDATA/MISC/TR1.TRI") == 0 && ok);
    CHECK(strcmp(rel_of("original/", "original/SIMDATA/MISC/TR1.TRI", &ok), "SIMDATA/MISC/TR1.TRI") == 0 && ok);
    CHECK(strcmp(rel_of("./original", "./original/a/b", &ok), "a/b") == 0 && ok);
    CHECK(strcmp(rel_of("/data/cd", "/data/cd/GAMEDATA/CONFIG/CONFIG.DAT", &ok), "GAMEDATA/CONFIG/CONFIG.DAT") == 0 && ok);
    CHECK(strcmp(rel_of("original", "original/a//b/./c", &ok), "a/b/c") == 0 && ok);   /* doubled slashes and dots */
    rel_of("original", "original", &ok);
    CHECK(ok);                                                                         /* the root itself */

    /* not under the root */
    rel_of("original", "originals/x", &ok); CHECK(!ok);
    rel_of("original", "other/x", &ok); CHECK(!ok);
    rel_of("original", "/etc/passwd", &ok); CHECK(!ok);
    rel_of("original", "", &ok); CHECK(!ok);
    /* unsafe below it */
    rel_of("original", "original/../etc/passwd", &ok); CHECK(!ok);
    rel_of("original", "original/a/../../x", &ok); CHECK(!ok);
    rel_of("original", "original/a/..", &ok); CHECK(!ok);
    rel_of("original", "original\\..\\x", &ok); CHECK(!ok);
    rel_of("original", "original/a\\b", &ok); CHECK(!ok);
    rel_of("original", "original/..a/b..", &ok); CHECK(ok);                           /* dots inside a name are names */
    CHECK(!modpath_relative(NULL, "a", (char[4]){0}, 4));
    char tiny[4];
    CHECK(!modpath_relative("original", "original/SIMDATA/MISC/TR1.TRI", tiny, sizeof tiny));   /* too long for the buffer */

    char out[64];
    CHECK(modpath_join("mods", "SIMDATA/x", out, sizeof out) && strcmp(out, "mods/SIMDATA/x") == 0);
    CHECK(modpath_join("mods/", "x", out, sizeof out) && strcmp(out, "mods/x") == 0);
    CHECK(!modpath_join("mods", "SIMDATA/x", out, 8));
    CHECK(modpath_upper("simdata/misc/tr1.tri", out, sizeof out) && strcmp(out, "SIMDATA/MISC/TR1.TRI") == 0);
    CHECK(!modpath_upper("abcdef", out, 4));

    CHECK(modpath_inside("/home/u/mod", "/home/u/mod"));
    CHECK(modpath_inside("/home/u/mod", "/home/u/mod/SIMDATA/x"));
    CHECK(modpath_inside("/home/u/mod/", "/home/u/mod/x"));
    CHECK(!modpath_inside("/home/u/mod", "/home/u/modern/x"));                         /* whole components */
    CHECK(!modpath_inside("/home/u/mod", "/home/u"));
    CHECK(!modpath_inside("/home/u/mod", "/etc/passwd"));
    CHECK(modpath_inside("/", "/anything"));
    return TEST_REPORT();
}
