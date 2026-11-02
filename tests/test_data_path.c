/* The game's own data files' search path (data_path.h): the program's base first, then the working directory, then a development build's source tree. */
#include "game/sys/data_path.h"
#include "tests/test.h"

#include <string.h>

#define CHECK_EQ_STR(a, b) CHECK(strcmp((a), (b)) == 0)

static void test_order_and_slashes(void)
{
    char p[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    CHECK_EQ_INT(data_path_candidates_for("/Applications/TNFS Restomod.app/Contents/Resources/", "/src/tnfs", "car_masks/P911/rsid_mask.png", p), 3);
    CHECK_EQ_STR(p[0], "/Applications/TNFS Restomod.app/Contents/Resources/data/car_masks/P911/rsid_mask.png");   /* the bundle's own, first */
    CHECK_EQ_STR(p[1], "data/car_masks/P911/rsid_mask.png");
    CHECK_EQ_STR(p[2], "/src/tnfs/data/car_masks/P911/rsid_mask.png");
    CHECK_EQ_INT(data_path_candidates_for("/opt/tnfs", NULL, "car_envmap.png", p), 2);   /* no trailing slash: added; a release build has no source tree */
    CHECK_EQ_STR(p[0], "/opt/tnfs/data/car_envmap.png");
    CHECK_EQ_STR(p[1], "data/car_envmap.png");
    CHECK_EQ_STR(p[2], "");
}

static void test_no_base_and_bad_input(void)
{
    char p[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    CHECK_EQ_INT(data_path_candidates_for(NULL, NULL, "x.png", p), 1);   /* the working directory alone */
    CHECK_EQ_STR(p[0], "");
    CHECK_EQ_STR(p[1], "data/x.png");
    CHECK_EQ_INT(data_path_candidates_for("", "", "x.png", p), 1);
    CHECK_EQ_INT(data_path_candidates_for("/b/", "/s", NULL, p), 0);
    CHECK_EQ_INT(data_path_candidates_for("/b/", "/s", "", p), 0);
    char longbase[DATA_PATH_MAX + 8];
    memset(longbase, 'a', sizeof longbase - 1), longbase[0] = '/', longbase[sizeof longbase - 1] = '\0';
    CHECK_EQ_INT(data_path_candidates_for(longbase, NULL, "x.png", p), 1);   /* too long to fit: skipped, the rest still there */
    CHECK_EQ_STR(p[0], "");
}

static void test_the_platform_base(void)
{
    char p[DATA_PATH_CANDIDATES][DATA_PATH_MAX];
    data_path_set_base("/Applications/X.app/Contents/Resources/");
    const int n = data_path_candidates("car_envmap.png", p);
    CHECK(n >= 2);
    CHECK_EQ_STR(p[0], "/Applications/X.app/Contents/Resources/data/car_envmap.png");
    data_path_set_base(NULL);
    CHECK_EQ_STR((data_path_candidates("car_envmap.png", p), p[0]), "");   /* no base: not tried */
}

int main(void)
{
    test_order_and_slashes();
    test_no_base_and_bad_input();
    test_the_platform_base();
    return TEST_REPORT();
}
