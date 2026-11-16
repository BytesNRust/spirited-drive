/* The version line at the Game Mode menu's foot: the version, and "Update available" once the check found a newer one. */
#include "game/frontend/version_footer.h"
#include "test.h"

static void test_the_version_alone(void)
{
    char line[64];
    CHECK_EQ_INT(version_footer_line(line, sizeof line, 0, "0.9.0", UPDATE_PENDING, ""), 1);
    CHECK(strcmp(line, "v0.9.0") == 0);
    CHECK_EQ_INT(version_footer_line(line, sizeof line, 0, "0.9.0", UPDATE_NONE, ""), 1);
    CHECK(strcmp(line, "v0.9.0") == 0);
}

static void test_a_newer_release_is_announced(void)
{
    char line[64];
    CHECK_EQ_INT(version_footer_line(line, sizeof line, 0, "0.9.0", UPDATE_NEWER, "0.9.1"), 1);
    CHECK(strcmp(line, "v0.9.0") == 0);
    CHECK_EQ_INT(version_footer_line(line, sizeof line, 1, "0.9.0", UPDATE_NEWER, "0.9.1"), 1);
    CHECK(strcmp(line, "Update available: v0.9.1") == 0);
    CHECK_EQ_INT(version_footer_line(line, sizeof line, 1, "0.9.0", UPDATE_NONE, ""), 0);
    CHECK_EQ_INT(version_footer_line(line, sizeof line, 1, "0.9.0", UPDATE_PENDING, ""), 0);
    CHECK_EQ_INT(version_footer_line(line, sizeof line, 2, "0.9.0", UPDATE_NEWER, "0.9.1"), 0);
}

static void test_a_short_buffer_is_cut_not_overrun(void)
{
    char line[8];
    CHECK_EQ_INT(version_footer_line(line, sizeof line, 0, "0.9.0-dev+abcdef0", UPDATE_NONE, ""), 1);
    CHECK_EQ_INT(strlen(line), 7);
    CHECK_EQ_INT(version_footer_line(NULL, 0, 0, "0.9.0", UPDATE_NONE, ""), 0);
    CHECK_EQ_INT(version_footer_line(line, sizeof line, 0, NULL, UPDATE_NONE, ""), 0);
}

static void test_an_empty_version_gives_no_line(void)
{
    char line[16] = "x";
    CHECK_EQ_INT(version_footer_line(line, sizeof line, 0, "", UPDATE_NONE, ""), 0);
    CHECK_EQ_INT(line[0], 0);
}

/* port (dep59 step 4): TNFS_VERSION_TEXT, set by tools/menu_walk.sh, stands for the build's version so two builds' Game Mode pictures
 * compare; an override that is empty, too long or not printable ASCII is ignored */
static void test_the_walk_names_the_version(void)
{
    CHECK(strcmp(version_footer_text("0.9.0-dev+abcdef0", "walk"), "walk") == 0);
    CHECK(strcmp(version_footer_text("0.9.0-dev+abcdef0", NULL), "0.9.0-dev+abcdef0") == 0);
    CHECK(strcmp(version_footer_text("0.9.0-dev+abcdef0", ""), "0.9.0-dev+abcdef0") == 0);
    CHECK(strcmp(version_footer_text("0.9.0", "0123456789012345678901234567890"), "0123456789012345678901234567890") == 0);   /* 31 */
    CHECK(strcmp(version_footer_text("0.9.0", "01234567890123456789012345678901"), "0.9.0") == 0);   /* 32: too long */
    CHECK(strcmp(version_footer_text("0.9.0", "a\tb"), "0.9.0") == 0);
    CHECK(strcmp(version_footer_text("0.9.0", "caf\xc3\xa9"), "0.9.0") == 0);
    char line[64];
    CHECK_EQ_INT(version_footer_line(line, sizeof line, 0, version_footer_text("0.9.0-dev+abcdef0", "walk"), UPDATE_NONE, ""), 1);
    CHECK(strcmp(line, "vwalk") == 0);
}

int main(void)
{
    test_the_walk_names_the_version();
    test_the_version_alone();
    test_a_newer_release_is_announced();
    test_a_short_buffer_is_cut_not_overrun();
    test_an_empty_version_gives_no_line();
    return TEST_REPORT();
}
