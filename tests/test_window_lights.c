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
    CHECK_EQ_INT(window_boards_for("AL2", &b), 0);
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

static void test_every_board_stays_inside_its_texture(void)
{
    static const char *const tracks[] = { "CY1", "CY2", "CY3", "TR2", "AL1" };
    static const int boards[] = { 1, 4, 1, 1, 1 };
    for (int t = 0; t < 5; t++) {
        const WindowBoard *b = NULL;
        CHECK_EQ_INT(window_boards_for(tracks[t], &b), boards[t]);
        for (int i = 0; i < boards[t]; i++) {
            CHECK(b[i].ncol >= 1 && b[i].ncol <= WINDOW_COLS_MAX && b[i].nrow >= 1 && b[i].nrow <= WINDOW_ROWS_MAX);
            for (int c = 0; c < b[i].ncol; c++) CHECK(b[i].col[c][0] >= 0 && b[i].col[c][0] <= b[i].col[c][1] && b[i].col[c][1] < b[i].width);
            for (int r = 0; r < b[i].nrow; r++) CHECK(b[i].row[r][0] >= 0 && b[i].row[r][0] <= b[i].row[r][1] && b[i].row[r][1] < b[i].height);
            for (int c = 1; c < b[i].ncol; c++) CHECK(b[i].col[c][0] > b[i].col[c - 1][1]);
            for (int r = 1; r < b[i].nrow; r++) CHECK(b[i].row[r][0] > b[i].row[r - 1][1]);
        }
    }
}

static void test_the_city_boards_by_number(void)
{
    const WindowBoard *b = NULL;
    CHECK_EQ_INT(window_boards_for("CY2", &b), 4);
    CHECK_EQ_INT(b[0].scenery, 25); CHECK_EQ_INT(b[1].scenery, 27); CHECK_EQ_INT(b[2].scenery, 29); CHECK_EQ_INT(b[3].scenery, 17);
    CHECK_EQ_INT(b[0].ncol, 6); CHECK_EQ_INT(b[0].nrow, 2);
    CHECK_EQ_INT(window_boards_for("CY3", &b), 1);
    CHECK_EQ_INT(b[0].scenery, 21); CHECK_EQ_INT(b[0].width, 32); CHECK_EQ_INT(b[0].height, 128);
}

/* by darkness: the board's windows are the dark patches of its wall */
enum { DW = 40, DH = 40, BRIGHT = 7, DARK = 9, EDGE = 255 };

static void dark_palette(uint8_t pal[768])
{
    memset(pal, 200, 768);
    pal[3 * DARK] = pal[3 * DARK + 1] = pal[3 * DARK + 2] = 20;
    pal[3 * EDGE] = 60, pal[3 * EDGE + 1] = 0, pal[3 * EDGE + 2] = 60;   /* the transparent magenta is dark too */
}

static void dark_board(uint8_t px[DW * DH])
{
    memset(px, BRIGHT, DW * DH);
    for (int w = 0; w < 6; w++)   /* six windows of 5 x 8, three over three */
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 5; x++) px[(4 + (w / 3) * 18 + y) * DW + 3 + (w % 3) * 12 + x] = DARK;
    px[38 * DW + 38] = DARK;   /* a speck, no window */
    for (int x = 0; x < DW; x++) px[39 * DW + x] = EDGE;
}

static const WindowDark DARKS = { 3, DW, DH, { 0, 0, DW - 1, DH - 2 }, 90, 3, 4, 12, 16, 255 };

static void test_dark_patches_are_the_windows(void)
{
    uint8_t px[DW * DH], pal[768], again[DW * DH], keep[DW * DH];
    dark_palette(pal), dark_board(px), dark_board(again), dark_board(keep);
    const size_t lit = window_dark_paint(&DARKS, px, DW, DH, pal, LIT, 5);
    CHECK(lit > 0 && lit < 6);   /* a share of the six, not the speck */
    size_t texels = 0;
    for (int i = 0; i < DW * DH; i++) {
        texels += px[i] == LIT;
        if (px[i] == LIT) CHECK(keep[i] == DARK);   /* only dark texels change */
        else CHECK(px[i] == keep[i]);
    }
    CHECK(texels == lit * 40);   /* each lit window is its 5 x 8 */
    CHECK(px[38 * DW + 38] == DARK);   /* the speck is no window */
    window_dark_paint(&DARKS, again, DW, DH, pal, LIT, 5);
    CHECK(memcmp(px, again, sizeof px) == 0);   /* the same seed, the same windows */
    window_dark_paint(&DARKS, px, DW, DH, pal, LIT, 5);
    CHECK(memcmp(px, again, sizeof px) == 0);   /* painting again changes nothing */
    uint8_t other[DW * DH];
    dark_board(other);
    window_dark_paint(&DARKS, other, DW, DH, pal, LIT, 77);
    CHECK(memcmp(px, other, sizeof px) != 0);   /* another seed, others */
    uint8_t small[8 * 8] = { 0 };
    CHECK_EQ_INT((int)window_dark_paint(&DARKS, small, 8, 8, pal, LIT, 5), 0);   /* another size is left alone */
}

/* a negative dark_luma looks for the light patches instead (a facade whose windows are pale on a dark wall) */
static void test_light_patches_are_the_windows_when_asked(void)
{
    uint8_t px[DW * DH], pal[768], keep[DW * DH];
    dark_palette(pal);
    memset(pal + 3 * BRIGHT, 20, 3);          /* the wall is dark ... */
    memset(pal + 3 * DARK, 220, 3);           /* ... the windows are the pale texels */
    dark_board(px), dark_board(keep);
    const WindowDark light = { 3, DW, DH, { 0, 0, DW - 1, DH - 2 }, -150, 3, 4, 12, 16, 255 };
    const size_t lit = window_dark_paint(&light, px, DW, DH, pal, LIT, 5);
    CHECK(lit > 0 && lit < 6);
    for (int i = 0; i < DW * DH; i++) {
        if (px[i] == LIT) CHECK(keep[i] == DARK);
        else CHECK(px[i] == keep[i]);
    }
}

static void test_the_dark_boards_of_each_track(void)
{
    const WindowDark *d = NULL;
    CHECK(window_darks_for("CL1", &d) >= 1 && d != NULL);
    CHECK_EQ_INT(window_darks_for("CY1", &d), 0);
    CHECK_EQ_INT(window_darks_for("CY3", &d), 2);
    CHECK_EQ_INT(window_darks_for("AL1", &d), 2);
    CHECK_EQ_INT(window_darks_for("TR4", &d), 3);
    CHECK_EQ_INT(window_darks_for("tr4", &d), 3);
    CHECK_EQ_INT(window_darks_for("TR7", &d), 1);
    CHECK_EQ_INT(window_boards_for("TR7", &(const WindowBoard *){ NULL }), 1);
    CHECK_EQ_INT(window_darks_for(NULL, &d), 0);
}

/* port (sign_glow): the City's matrix sign (CY1 board 7, 128 x 63): its amber bulbs are the lit texels, the dark grid round them is left alone */
static void test_the_matrix_sign_lights_its_amber_bulbs(void)
{
    const WindowSign *s = NULL;
    CHECK_EQ_INT(window_signs_for("CY1", &s), 1);
    CHECK_EQ_INT(s[0].scenery, 7);
    CHECK_EQ_INT(window_signs_for("cy1", &s), 1);
    CHECK_EQ_INT(s[0].width, 128);
    CHECK_EQ_INT(s[0].height, 63);
    CHECK_EQ_INT(window_signs_for("CY2", &s), 0);   /* no such sign there, and CY3's copy is placed by no node */
    CHECK_EQ_INT(window_signs_for("CY3", &s), 0);
    CHECK_EQ_INT(window_signs_for(NULL, &s), 0);
    window_signs_for("CY1", &s);
    uint8_t palette[256 * 3] = { 0 };
    palette[3 * 1] = 12, palette[3 * 1 + 1] = 12, palette[3 * 1 + 2] = 12;      /* the grid */
    palette[3 * 2] = 170, palette[3 * 2 + 1] = 145, palette[3 * 2 + 2] = 20;    /* a bulb's body */
    palette[3 * 3] = 246, palette[3 * 3 + 1] = 226, palette[3 * 3 + 2] = 0;     /* a bulb's centre */
    palette[3 * 4] = 140, palette[3 * 4 + 1] = 20, palette[3 * 4 + 2] = 20;     /* a red, not amber */
    static uint8_t px[128 * 63], again[128 * 63];
    for (int i = 0; i < 128 * 63; i++) px[i] = (uint8_t)(1 + i % 4);
    memcpy(again, px, sizeof px);
    const size_t lit = window_sign_paint(&s[0], px, 128, 63, palette, 9);
    size_t expect = 0;
    for (int i = 0; i < 128 * 63; i++) expect += again[i] == 2 || again[i] == 3;
    CHECK_EQ_INT((int)lit, (int)expect);
    for (int i = 0; i < 128 * 63; i++) CHECK_EQ_INT(px[i], again[i] == 2 || again[i] == 3 ? 9 : again[i]);   /* only the amber ones change */
    CHECK_EQ_INT((int)window_sign_paint(&s[0], px, 128, 63, palette, 9), 0);   /* painting again changes nothing */
    CHECK_EQ_INT((int)window_sign_paint(&s[0], again, 64, 63, palette, 9), 0);   /* a board of another size is left alone */
}

int main(void)
{
    test_the_matrix_sign_lights_its_amber_bulbs();
    test_dark_patches_are_the_windows();
    test_light_patches_are_the_windows_when_asked();
    test_the_dark_boards_of_each_track();
    test_every_board_stays_inside_its_texture();
    test_the_city_boards_by_number();
    test_the_boards_of_each_track();
    test_a_seeded_subset_is_lit_inside_the_windows();
    test_a_board_of_another_size_is_left_alone();
    return TEST_REPORT();
}
