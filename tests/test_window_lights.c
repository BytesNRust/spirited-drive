/* The lit windows of the night's buildings (window_lights.h): which boards a track has, the seeded subset, what the paint touches, idempotence. */
#include "game/race/window_lights.h"
#include "tests/test.h"

#include <string.h>

enum { W = 63, H = 63, WALL = 40, LIT = 200 };

static void board(uint8_t px[W * H]) { memset(px, WALL, W * H); }

static void test_the_boards_of_each_track(void)
{
    const WindowBoard *b = NULL;
    CHECK(window_boards_for("CY1", &b) >= 1 && b != NULL);
    CHECK_EQ_INT(b[0].scenery, 40);
    CHECK_EQ_INT(b[0].width, 63);
    CHECK_EQ_INT(b[0].height, 63);
    CHECK(window_boards_for("cy1", &b) >= 1);
    CHECK_EQ_INT(window_boards_for("AL1", &b), 0);
    CHECK_EQ_INT(window_boards_for(NULL, &b), 0);
}

static void test_a_seeded_subset_is_lit_inside_the_windows(void)
{
    const WindowBoard *b = NULL;
    window_boards_for("CY1", &b);
    uint8_t px[W * H], again[W * H];
    board(px), board(again);
    const size_t lit = window_board_paint(&b[0], px, W, H, LIT, 5);
    CHECK(lit > 0);
    size_t texels = 0;
    for (int i = 0; i < W * H; i++) texels += px[i] == LIT;
    CHECK(texels >= lit);
    const size_t windows = (size_t)b[0].ncol * (size_t)b[0].nrow;
    CHECK(lit < windows && lit * 10 > windows);   /* a subset: some, not all, not a handful */
    for (int x = 0; x < W; x++) CHECK(px[x] == WALL);   /* nothing above the first row of windows */
    CHECK_EQ_INT((int)window_board_paint(&b[0], again, W, H, LIT, 5), (int)lit);   /* the same seed, the same windows */
    CHECK(memcmp(px, again, sizeof px) == 0);
    board(again);
    window_board_paint(&b[0], again, W, H, LIT, 6);
    CHECK(memcmp(px, again, sizeof px) != 0);   /* another seed, others */
    uint8_t twice[W * H];
    memcpy(twice, px, sizeof twice);
    window_board_paint(&b[0], twice, W, H, LIT, 5);
    CHECK(memcmp(px, twice, sizeof px) == 0);   /* painting again changes nothing */
}

static void test_a_board_of_another_size_is_left_alone(void)
{
    const WindowBoard *b = NULL;
    window_boards_for("CY1", &b);
    uint8_t px[32 * 32];
    memset(px, WALL, sizeof px);
    CHECK_EQ_INT((int)window_board_paint(&b[0], px, 32, 32, LIT, 5), 0);
    for (size_t i = 0; i < sizeof px; i++) CHECK(px[i] == WALL);
}

int main(void)
{
    test_the_boards_of_each_track();
    test_a_seeded_subset_is_lit_inside_the_windows();
    test_a_board_of_another_size_is_left_alone();
    return TEST_REPORT();
}
