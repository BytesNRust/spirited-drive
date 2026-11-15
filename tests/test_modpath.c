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

/* a case-sensitive file system: the files are the names listed, compared exactly; every path asked is counted */
typedef struct {
    const char *const *files;
    int asked;
} FakeFs;

static int fake_is_file(const char *path, void *ctx)
{
    FakeFs *fs = ctx;
    fs->asked++;
    for (const char *const *f = fs->files; *f != NULL; ++f)
        if (strcmp(*f, path) == 0) return 1;
    return 0;
}

/* modpath_find on that system: which of the forms finds `rel`, in the fixed order as asked, UPPER, lower */
static void test_find(void)
{
    char out[128], name[64];
    static const char *const lower_only[] = { "mods/simdata/misc/tr2.tri", NULL };
    FakeFs fs = { lower_only, 0 };
    /* the upper-case retry (resource_load's) of a mod made with lower-case names: found by the lower-case form */
    CHECK(modpath_find("mods", "SIMDATA/MISC/TR2.TRI", fake_is_file, &fs, out, sizeof out, name, sizeof name));
    CHECK(strcmp(out, "mods/simdata/misc/tr2.tri") == 0 && strcmp(name, "simdata/misc/tr2.tri") == 0);
    CHECK_EQ_INT(fs.asked, 2);   /* as asked (= UPPER, not asked twice), then lower */

    static const char *const upper_only[] = { "mods/SIMDATA/MISC/TR2.TRI", NULL };
    fs = (FakeFs){ upper_only, 0 };
    CHECK(modpath_find("mods", "simdata/misc/tr2.tri", fake_is_file, &fs, out, sizeof out, name, sizeof name));
    CHECK(strcmp(name, "SIMDATA/MISC/TR2.TRI") == 0);
    CHECK_EQ_INT(fs.asked, 2);   /* as asked (= lower), then UPPER */

    static const char *const mixed[] = { "mods/Simdata/Misc/Tr2.tri", "mods/SIMDATA/MISC/TR2.TRI", "mods/simdata/misc/tr2.tri", NULL };
    fs = (FakeFs){ mixed, 0 };
    CHECK(modpath_find("mods", "Simdata/Misc/Tr2.tri", fake_is_file, &fs, out, sizeof out, name, sizeof name));
    CHECK(strcmp(name, "Simdata/Misc/Tr2.tri") == 0 && fs.asked == 1);   /* as asked comes first */
    static const char *const two[] = { "mods/SIMDATA/MISC/TR2.TRI", "mods/simdata/misc/tr2.tri", NULL };
    fs = (FakeFs){ two, 0 };
    CHECK(modpath_find("mods", "Simdata/Misc/Tr2.tri", fake_is_file, &fs, out, sizeof out, name, sizeof name));
    CHECK(strcmp(name, "SIMDATA/MISC/TR2.TRI") == 0);   /* then UPPER, before lower */
    CHECK_EQ_INT(fs.asked, 2);

    static const char *const none[] = { "mods/other.tri", NULL };
    fs = (FakeFs){ none, 0 };
    CHECK(!modpath_find("mods", "Simdata/tr2.tri", fake_is_file, &fs, out, sizeof out, name, sizeof name));
    CHECK_EQ_INT(fs.asked, 3);   /* the three forms, then nothing */
    CHECK(!modpath_find("mods", "tr2.tri", fake_is_file, &fs, out, sizeof out, name, 4));   /* the name does not fit */
    CHECK(!modpath_find("mods", "tr2.tri", NULL, &fs, out, sizeof out, name, sizeof name));
}

int main(void)
{
    test_find();
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

    CHECK(modpath_inside("/home/u/mod", "/home/u/mod"));
    CHECK(modpath_inside("/home/u/mod", "/home/u/mod/SIMDATA/x"));
    CHECK(modpath_inside("/home/u/mod/", "/home/u/mod/x"));
    CHECK(!modpath_inside("/home/u/mod", "/home/u/modern/x"));                         /* whole components */
    CHECK(!modpath_inside("/home/u/mod", "/home/u"));
    CHECK(!modpath_inside("/home/u/mod", "/etc/passwd"));
    CHECK(modpath_inside("/", "/anything"));
    return TEST_REPORT();
}
