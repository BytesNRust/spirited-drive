/* The game's own data files' search path (data_path.h): the program's base first, then the working directory, then a development build's source tree. */
#include "game/sys/data_path.h"
#include "tests/test.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK_EQ_STR(a, b) CHECK(strcmp((a), (b)) == 0)

static void test_order_and_slashes(void)
{
    char p[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    CHECK_EQ_INT(data_path_candidates_for("/mods", "/Applications/Spirited Drive.app/Contents/Resources/", "/src/tnfs", "car_masks/P911/rsid_mask.png", p), 4);
    CHECK_EQ_STR(p[0], "/mods/car_masks/P911/rsid_mask.png");   /* a mod's own copy, first, with no data/ in it */
    CHECK_EQ_STR(p[1], "/Applications/Spirited Drive.app/Contents/Resources/data/car_masks/P911/rsid_mask.png");   /* then the bundle's own */
    CHECK_EQ_STR(p[2], "data/car_masks/P911/rsid_mask.png");
    CHECK_EQ_STR(p[3], "/src/tnfs/data/car_masks/P911/rsid_mask.png");
    CHECK_EQ_INT(data_path_candidates_for("/mods/", NULL, NULL, "sounds/thunder.wav", p), 2);   /* a trailing slash is not doubled */
    CHECK_EQ_STR(p[0], "/mods/sounds/thunder.wav");
    CHECK_EQ_INT(data_path_candidates_for(NULL, "/opt/tnfs", NULL, "car_envmap.png", p), 2);   /* no trailing slash: added; a release build has no source tree; no mod: no mod's */
    CHECK_EQ_STR(p[0], "");
    CHECK_EQ_STR(p[1], "/opt/tnfs/data/car_envmap.png");
    CHECK_EQ_STR(p[2], "data/car_envmap.png");
    CHECK_EQ_STR(p[3], "");
}

static void test_no_base_and_bad_input(void)
{
    char p[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    CHECK_EQ_INT(data_path_candidates_for(NULL, NULL, NULL, "x.png", p), 1);   /* the working directory alone */
    CHECK_EQ_STR(p[1], "");
    CHECK_EQ_STR(p[2], "data/x.png");
    CHECK_EQ_INT(data_path_candidates_for("", "", "", "x.png", p), 1);
    CHECK_EQ_INT(data_path_candidates_for(NULL, "/b/", "/s", NULL, p), 0);
    CHECK_EQ_INT(data_path_candidates_for(NULL, "/b/", "/s", "", p), 0);
    char longbase[DATA_PATH_MAX + 8];
    memset(longbase, 'a', sizeof longbase - 1), longbase[0] = '/', longbase[sizeof longbase - 1] = '\0';
    CHECK_EQ_INT(data_path_candidates_for(NULL, longbase, NULL, "x.png", p), 1);   /* too long to fit: skipped, the rest still there */
    CHECK_EQ_STR(p[1], "");
}

static void test_the_platform_base(void)
{
    char p[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    data_path_set_base("/Applications/X.app/Contents/Resources/");
    const int n = data_path_candidates("car_envmap.png", p);
    CHECK(n >= 2);
    CHECK_EQ_STR(p[1], "/Applications/X.app/Contents/Resources/data/car_envmap.png");
    CHECK_EQ_STR(p[0], "");   /* no mods folder yet */
    data_path_set_mods(NULL, 0);
    data_path_candidates("car_envmap.png", p);
    CHECK_EQ_STR(p[0], "");
    data_path_set_base(NULL);
    CHECK_EQ_STR((data_path_candidates("car_envmap.png", p), p[1]), "");   /* no base: not tried */
}

/* i191: a candidate is tried only when the file is there; a folder is not a file */
static int not_the_first(int index, const char *relative) { (void)relative; return index != 0; }

static void test_exists(void)
{
    const char *const path = "data_path_exists_test.txt";
    remove(path);
    CHECK(!data_path_exists(path));
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) fputs("x", f), fclose(f);
    CHECK(data_path_exists(path));
    remove(path);
    CHECK(!data_path_exists("."));
    CHECK(!data_path_exists(""));
    CHECK(!data_path_exists(NULL));
}

static void put(const char *dir, const char *name)
{
    char path[256];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    mkdir(dir, 0777);
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) fputs("x", f), fclose(f);
}

/* dep59 n4: several mods, highest first: the mod's candidate is the first of them that has the file; none has it, none */
static void test_several_mods(void)
{
    char p[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    const char *const a = "data_path_mod_a", *const b = "data_path_mod_b";
    put(a, "both.png"), put(b, "both.png"), put(b, "only_b.png");
    const char *const dirs[] = { a, b };
    data_path_set_mods(dirs, 2);
    data_path_candidates("both.png", p);
    CHECK_EQ_STR(p[0], "data_path_mod_a/both.png");
    data_path_candidates("only_b.png", p);
    CHECK_EQ_STR(p[0], "data_path_mod_b/only_b.png");
    data_path_candidates("none.png", p);
    CHECK_EQ_STR(p[0], "");
    const char *const swapped[] = { b, a };
    data_path_set_mods(swapped, 2);
    data_path_candidates("both.png", p);
    CHECK_EQ_STR(p[0], "data_path_mod_b/both.png");
    data_path_set_serve(not_the_first);   /* dep59 step 2: a folder that may not serve a file is passed over */
    data_path_candidates("both.png", p);
    CHECK_EQ_STR(p[0], "data_path_mod_a/both.png");
    data_path_candidates("only_b.png", p);
    CHECK_EQ_STR(p[0], "");
    data_path_set_serve(NULL);
    data_path_set_mods(NULL, 0);
    data_path_candidates("both.png", p);
    CHECK_EQ_STR(p[0], "");
    remove("data_path_mod_a/both.png"), remove("data_path_mod_b/both.png"), remove("data_path_mod_b/only_b.png");
    rmdir(a), rmdir(b);
}

int main(void)
{
    test_exists();
    test_order_and_slashes();
    test_no_base_and_bad_input();
    test_the_platform_base();
    test_several_mods();
    return TEST_REPORT();
}
