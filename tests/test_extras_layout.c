/* The EXTRAS screen (extras_menu.c), split from test_extras_menu.c: the groups, their headers and the title, the rows under them, the
 * layout the pages share, and the words drawn through the smooth op or without it. Same rig (extras_rig.h). */
#include "extras_rig.h"

/* port: the owner's four groups, as the two headers over them on each page, in the owner's names. */
static void test_the_groups_have_headers(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK(shown(&r.fake, "Cheats"));
    CHECK(shown(&r.fake, "Unlocks & mods"));

    Rig two;
    setup(&two);
    int turn[48];
    int n = keys_to_row(turn, ROW_FIRST_P2);
    turn[n++] = ESCAPE;
    run(&two, turn, n);
    CHECK(shown(&two.fake, "In-race options"));
    CHECK(!shown(&two.fake, "Miscellaneous"));   /* it heads page 3 now */
    /* page 1's headers are not page 2's: the two pages name different groups, and each redraw paints the whole page,
     * so what is left on screen after turning is page 2 alone */
    CHECK(!shown(&two.fake, "Cheats"));
    CHECK(!shown(&two.fake, "Unlocks & mods"));
}

/* the headers are the title's red, the rows cream or yellow as they are lit: the headers name a group, they are not a
 * row, so they never take the selection. */
static void test_headers_are_red_and_never_lit(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK_EQ_INT(style_in(&r.fake, "Cheats"), STYLE_RED);
    CHECK_EQ_INT(style_in(&r.fake, "Unlocks & mods"), STYLE_RED);
    CHECK_EQ_INT(style_in(&r.fake, "Explode mode"), STYLE_YELLOW);   /* the first row is selected */
    CHECK_EQ_INT(style_in(&r.fake, "Nitro mode"), STYLE_CREAM);
}

/* The rows sit under the header of their group, on each page: a group's rows are the ones between its header and the
 * next one (or the Done band). */
static void test_the_rows_sit_under_their_groups(void)
{
    Rig one;
    setup(&one);
    const int here[] = { ESCAPE };
    run(&one, here, 1);
    CHECK(y_in(&one.fake, "Cheats") < y_in(&one.fake, "Explode mode"));
    CHECK(y_in(&one.fake, "Explode mode") < y_in(&one.fake, "Warrior Player Car"));
    CHECK(y_in(&one.fake, "Warrior Player Car") < y_in(&one.fake, "Unlocks & mods"));
    CHECK(y_in(&one.fake, "Unlocks & mods") < y_in(&one.fake, "Warrior Rival Car"));
    CHECK(y_in(&one.fake, "Warrior Rival Car") < y_in(&one.fake, "Unlock Level"));

    Rig two;
    setup(&two);
    int turn[48];
    int n = keys_to_row(turn, ROW_FIRST_P2);
    turn[n++] = ESCAPE;
    run(&two, turn, n);
    CHECK(y_in(&two.fake, "In-race options") < y_in(&two.fake, "Speed units"));
    CHECK(y_in(&two.fake, "Speed units") < y_in(&two.fake, "3D render stats"));
    CHECK(y_in(&two.fake, "3D render stats") < y_in(&two.fake, "Dash box"));
    CHECK(y_in(&two.fake, "Dash box") < y_in(&two.fake, "Difficulty"));
    Rig three;
    setup(&three);
    n = keys_to_row(turn, ROW_FIRST_P3);
    turn[n++] = ESCAPE;
    run(&three, turn, n);
    CHECK(y_in(&three.fake, "Miscellaneous") < y_in(&three.fake, "Start Screen"));
    CHECK(y_in(&three.fake, "Start Screen") < y_in(&three.fake, "Intro video"));
    CHECK(y_in(&three.fake, "Intro video") < y_in(&three.fake, "Updates and anonymous stats"));
    CHECK(y_in(&three.fake, "Updates and anonymous stats") < y_in(&three.fake, "Open the log folder"));
}

/* The headers sit in the same place on both pages, the rows are one pitch apart, and there is a visible gap before the
 * second group: the same layout for both pages, which is what makes them read as two columns of one list. */
static void test_both_pages_share_one_layout(void)
{
    Rig one;
    setup(&one);
    const int here[] = { ESCAPE };
    run(&one, here, 1);

    Rig two;
    setup(&two);
    int turn[48];
    int n = keys_to_row(turn, ROW_FIRST_P2);
    turn[n++] = ESCAPE;
    run(&two, turn, n);

    /* the first header of each page is line 0; the second header is line 5 on page 1 and line 11 on page 2 (the lines
     * are PITCH apart, with the gap of 14 added from the second header down) */
    CHECK_EQ_INT(y_in(&two.fake, "In-race options"), y_in(&one.fake, "Cheats"));
    CHECK_EQ_INT(y_in(&one.fake, "Unlocks & mods"), 100 + 5 * PITCH + 14);
    Rig three;
    setup(&three);
    n = keys_to_row(turn, ROW_FIRST_P3);
    turn[n++] = ESCAPE;
    run(&three, turn, n);
    CHECK_EQ_INT(y_in(&three.fake, "Miscellaneous"), y_in(&one.fake, "Cheats"));   /* pages 2 and 3 have one header, on the first line */
    CHECK_EQ_INT(y_in(&three.fake, "Start Screen"), y_in(&two.fake, "Speed units"));
    /* the pitch: one row a line below the last */
    CHECK_EQ_INT(y_in(&one.fake, "Nitro mode") - y_in(&one.fake, "Explode mode"), PITCH);
    CHECK_EQ_INT(y_in(&two.fake, "No driving assists") - y_in(&two.fake, "Speed units"), PITCH);
    /* the gap: the second header starts further down than the first group's last row, by more than a row's pitch */
    CHECK_EQ_INT(y_in(&one.fake, "Unlocks & mods") - y_in(&one.fake, "Warrior Player Car"), PITCH + 14);
}

/* The page must fit between the title and the DONE/CANCEL band without covering either: the last row's text has to end
 * above the background's own buttons (430), and the first header has to start below the title. */
static void test_the_page_fits_between_the_title_and_the_buttons(void)
{
    enum { TITLE_BOTTOM = 96, BUTTONS_TOP = 430 };
    Rig one;
    setup(&one);
    const int here[] = { ESCAPE };
    run(&one, here, 1);
    /* the page's first and last words, as the text's own top and its bottom */
    CHECK(y_in(&one.fake, "Cheats") >= TITLE_BOTTOM);
    CHECK(y_in(&one.fake, "Car Colours") + LINE < BUTTONS_TOP);
    CHECK(y_in(&one.fake, "Unlock Level") < y_in(&one.fake, "Car Colours"));   /* the group's last row */

    Rig two;
    setup(&two);
    int turn[48];
    int n = keys_to_row(turn, ROW_FIRST_P2);
    turn[n++] = ESCAPE;
    run(&two, turn, n);
    CHECK(y_in(&two.fake, "In-race options") >= TITLE_BOTTOM);
    CHECK(y_in(&two.fake, "Difficulty") + LINE < BUTTONS_TOP);
    Rig three;
    setup(&three);
    n = keys_to_row(turn, ROW_FIRST_P3);
    turn[n++] = ESCAPE;
    run(&three, turn, n);
    CHECK(y_in(&three.fake, "Open the log folder") + LINE < BUTTONS_TOP);
}

/* port: the screen names itself, in the words the player just clicked in Game Mode and Options. It sits on the band's
 * the clean background leaves the burnt-in title in, so it reads as that title. */
static void test_the_screen_is_titled_customize_game(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK(shown(&r.fake, "Customize Game"));
}

/* port: the title in the original screens' look, through its own op (red, at their size), not as a row */
static void test_the_title_goes_through_the_title_op(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK(r.fake.titles > 0);
    CHECK_EQ_INT(style_in(&r.fake, "Customize Game"), 2);
}

/* port: the groups' headers in the credits' look, underlined across the rows, labels to values */
static void test_the_headers_go_through_the_header_op(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK_EQ_INT(r.fake.headers, 2);
    CHECK_EQ_INT(r.fake.header_width, 600 - 16);
}

/* port: the words go through the screen's smooth draw (the menu art's letters), the nav included; only the title has
 * its own op. Every redraw repaints the whole page first, so nothing has to be put back under them. */
static void test_text_is_drawn_over_the_background(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK(r.fake.smooth_draws > 0);
    CHECK_EQ_INT(r.fake.smooth_draws + r.fake.titles + r.fake.headers, r.fake.drawn_count);
}

/* port: without a smooth op to call the screen still draws every word, the way the original's rows do. */
static void test_rows_are_drawn_without_a_smooth_op(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run_with(&r, keys, 1, NULL);
    CHECK_EQ_INT(r.fake.smooth_draws, 0);
    CHECK(shown(&r.fake, "Cheats"));
    CHECK(shown(&r.fake, "Explode mode"));
}

/* port: the owner's blank line on page 3: Start Screen and Intro video, an empty line, then Updates, the log folder and
 * Restore. The rows keep the pitch, the blank is one more of it, the page still ends above DONE/CANCEL, and the blank is
 * never the selection: Down from Intro video lands on Updates, Up from Updates on Intro video. */
static void test_page_3_has_a_blank_line_after_intro_video(void)
{
    enum { BUTTONS_TOP = 430 };
    Rig r;
    int keys[64];
    int n = keys_to_row(keys, ROW_MOVIES);
    keys[n++] = DOWN;                                    /* Intro video to the next row: Updates */
    keys[n++] = ESCAPE;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(style_in(&r.fake, "Updates and anonymous stats"), STYLE_YELLOW);
    CHECK_EQ_INT(style_in(&r.fake, "Intro video"), STYLE_CREAM);
    CHECK_EQ_INT(y_in(&r.fake, "Intro video") - y_in(&r.fake, "Start Screen"), PITCH);
    CHECK_EQ_INT(y_in(&r.fake, "Updates and anonymous stats") - y_in(&r.fake, "Intro video"), 2 * PITCH);   /* the blank */
    CHECK_EQ_INT(y_in(&r.fake, "Open the log folder") - y_in(&r.fake, "Updates and anonymous stats"), PITCH);
    CHECK_EQ_INT(y_in(&r.fake, "Restore profile defaults") - y_in(&r.fake, "Open the log folder"), PITCH);
    CHECK(y_in(&r.fake, "Restore profile defaults") + LINE <= BUTTONS_TOP);

    Rig up;
    n = keys_to_row(keys, ROW_UPDATE);
    keys[n++] = UP;                                      /* Updates back up: Intro video, over the blank */
    keys[n++] = ESCAPE;
    setup(&up);
    run(&up, keys, n);
    CHECK_EQ_INT(style_in(&up.fake, "Intro video"), STYLE_YELLOW);
    CHECK_EQ_INT(style_in(&up.fake, "Updates and anonymous stats"), STYLE_CREAM);
}

int main(void)
{
    test_the_groups_have_headers();
    test_headers_are_red_and_never_lit();
    test_the_rows_sit_under_their_groups();
    test_both_pages_share_one_layout();
    test_the_page_fits_between_the_title_and_the_buttons();
    test_the_screen_is_titled_customize_game();
    test_the_title_goes_through_the_title_op();
    test_the_headers_go_through_the_header_op();
    test_text_is_drawn_over_the_background();
    test_rows_are_drawn_without_a_smooth_op();
    test_page_3_has_a_blank_line_after_intro_video();
    return TEST_REPORT();
}
