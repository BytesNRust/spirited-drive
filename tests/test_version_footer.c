/* The version line at the Game Mode menu's foot: the version, and "Update available" once the check found a newer one. */
#include "game/frontend/version_footer.h"
#include "test.h"

static void test_the_version_alone(void)
{
    char line[64];
    CHECK_EQ_INT(version_footer_line(line, sizeof line, "0.9.0", UPDATE_PENDING, ""), 1);
    CHECK(strcmp(line, "v0.9.0") == 0);
    CHECK_EQ_INT(version_footer_line(line, sizeof line, "0.9.0", UPDATE_NONE, ""), 1);
    CHECK(strcmp(line, "v0.9.0") == 0);
}

static void test_a_newer_release_is_announced(void)
{
    char line[64];
    CHECK_EQ_INT(version_footer_line(line, sizeof line, "0.9.0", UPDATE_NEWER, "0.9.1"), 1);
    CHECK(strcmp(line, "v0.9.0 - Update available: v0.9.1") == 0);
}

static void test_a_short_buffer_is_cut_not_overrun(void)
{
    char line[8];
    CHECK_EQ_INT(version_footer_line(line, sizeof line, "0.9.0-dev+abcdef0", UPDATE_NONE, ""), 1);
    CHECK_EQ_INT(strlen(line), 7);
    CHECK_EQ_INT(version_footer_line(NULL, 0, "0.9.0", UPDATE_NONE, ""), 0);
    CHECK_EQ_INT(version_footer_line(line, sizeof line, NULL, UPDATE_NONE, ""), 0);
}

static void test_an_empty_version_gives_no_line(void)
{
    char line[16] = "x";
    CHECK_EQ_INT(version_footer_line(line, sizeof line, "", UPDATE_NONE, ""), 0);
    CHECK_EQ_INT(line[0], 0);
}

int main(void)
{
    test_the_version_alone();
    test_a_newer_release_is_announced();
    test_a_short_buffer_is_cut_not_overrun();
    test_an_empty_version_gives_no_line();
    return TEST_REPORT();
}
