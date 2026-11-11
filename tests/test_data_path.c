/* The game's own data files' search path (data_path.h): the program's base first, then the working directory, then a development build's source tree. */
#include "game/sys/data_path.h"
#include "tests/test.h"

#include <stdio.h>
#include <string.h>

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
    data_path_set_mod("/u/mods");
    data_path_candidates("car_envmap.png", p);
    CHECK_EQ_STR(p[0], "/u/mods/car_envmap.png");
    data_path_set_mod(NULL);
    data_path_set_base(NULL);
    CHECK_EQ_STR((data_path_candidates("car_envmap.png", p), p[1]), "");   /* no base: not tried */
}

/* i191: a candidate is tried only when the file is there; a folder is not a file */
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

int main(void)
{
    test_exists();
    test_order_and_slashes();
    test_no_base_and_bad_input();
    test_the_platform_base();
    return TEST_REPORT();
}
