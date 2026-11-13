/* The EXTRAS screen (extras_menu.c), split from test_extras_menu.c: where the selection comes from. The pointer's is
 * what is under it and nothing else, the keys' starts at the first row, a click acts on what it is on, and the page
 * indicator's triangles light alone and turn the page their way. Same rig (extras_rig.h). */
#include "extras_rig.h"

/* page 1's places, in the screen's 640x480: the rows of its first group sit one pitch apart from y 120 (Explode), the
 * buttons and the page indicator share the band at the bottom, and the title band above the rows is on nothing */
enum { ROW_X = 56, EXPLODE_Y = 130, NITRO_Y = 150, WARRIOR_Y = 190, BUTTONS_Y = 454, DONE_X = 60, NOWHERE_X = 600,
       NOWHERE_Y = 30, NAV_LEFT_X = 290, NAV_RIGHT_X = 350 };

/* the words drawn lit in the last frame */
static int lit_words(const Fake *f)
{
    int n = 0;
    for (int i = 0; i < f->drawn_count; i++) n += f->style_of[i] == STYLE_YELLOW;
    return n;
}

/* opened with the pointer on nothing, nothing is selected: no row lit */
static void test_nothing_is_lit_with_the_pointer_on_nothing(void)
{
    Rig r;
    const int keys[] = { IDLE, IDLE, ESCAPE };
    setup(&r);
    run(&r, keys, 3);
    CHECK(shown(&r.fake, "Explode mode"));
    CHECK_EQ_INT(style_in(&r.fake, "Explode mode"), STYLE_CREAM);
    CHECK_EQ_INT(lit_words(&r.fake), 0);
}

/* opened with the pointer on a row, that row is the selection, and the first row is not */
static void test_the_row_under_the_pointer_is_lit(void)
{
    Rig r;
    const int keys[] = { AT(ROW_X, WARRIOR_Y), IDLE, ESCAPE };
    setup(&r);
    run(&r, keys, 3);
    CHECK_EQ_INT(style_in(&r.fake, "Warrior Player Car"), STYLE_YELLOW);
    CHECK_EQ_INT(style_in(&r.fake, "Explode mode"), STYLE_CREAM);
}

/* the pointer moved off the rows onto nothing takes the selection with it */
static void test_the_pointer_off_the_rows_selects_nothing(void)
{
    Rig r;
    const int keys[] = { AT(ROW_X, WARRIOR_Y), IDLE, AT(NOWHERE_X, NOWHERE_Y), IDLE, ESCAPE };
    setup(&r);
    run(&r, keys, 5);
    CHECK_EQ_INT(lit_words(&r.fake), 0);
}

/* the keys start at the first row: the first one lights it and changes nothing, even Right */
static void test_the_first_key_lights_the_first_row_only(void)
{
    Rig r;
    const int keys[] = { RIGHT, IDLE, ESCAPE };
    setup(&r);
    run(&r, keys, 3);
    CHECK_EQ_INT(style_in(&r.fake, "Explode mode"), STYLE_YELLOW);

    Rig saved;
    int script[48];
    int n = 0;
    script[n++] = RIGHT;                                  /* the first row lit, Explode as it was */
    n = downs(script, n, ROWS_P1);                        /* Done */
    script[n++] = ENTER;
    setup(&saved);
    run(&saved, script, n);
    CHECK_EQ_INT(saved.fake.saves, 1);
    CHECK_EQ_INT(saved.fake.saved.explode, saved.e.explode);
    CHECK_EQ_INT(saved.fake.saved.explode, port_extras_defaults().explode);
}

/* a click acts on what is under the pointer: on a row it changes that row, on Done it saves */
static void test_a_click_acts_on_the_row_under_it(void)
{
    Rig r;
    const int keys[] = { AT(ROW_X, EXPLODE_Y), CLICK, IDLE, AT(DONE_X, BUTTONS_Y), CLICK, IDLE };
    setup(&r);
    const int before = r.e.explode;
    run(&r, keys, 6);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.explode, !before);
}

/* a click with the pointer on nothing does nothing, though the keys had a row selected before the pointer moved */
static void test_a_click_on_nothing_does_nothing(void)
{
    Rig r;
    int keys[48];
    int n = 0;
    keys[n++] = DOWN, keys[n++] = DOWN;                   /* the keys on Nitro */
    keys[n++] = AT(NOWHERE_X, NOWHERE_Y), keys[n++] = CLICK, keys[n++] = IDLE;
    keys[n++] = DOWN;                                     /* the first row again */
    n = downs(keys, n, ROWS_P1);                          /* Done */
    keys[n++] = ENTER;
    setup(&r);
    const PortExtras input = r.e;
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.nitro, input.nitro);
    CHECK_EQ_INT(r.fake.saved.explode, input.explode);
}

/* the pointer on the page indicator lights its triangle and leaves no row or button lit beside it */
static void test_the_page_indicator_lights_alone(void)
{
    Rig r;
    const int keys[] = { DOWN, DOWN, AT(NAV_RIGHT_X, BUTTONS_Y), IDLE, ESCAPE };
    setup(&r);
    run(&r, keys, 5);
    CHECK_EQ_INT(style_in(&r.fake, "Nitro mode"), STYLE_CREAM);
    CHECK_EQ_INT(lit_words(&r.fake), 0);
}

/* a click on the indicator turns the page and changes nothing: the row the keys were on stays as it was */
static void test_a_click_on_the_indicator_only_turns(void)
{
    Rig r;
    const int keys[] = { DOWN, DOWN, AT(NAV_RIGHT_X, BUTTONS_Y), CLICK, IDLE, IDLE, AT(DONE_X, BUTTONS_Y), CLICK,
                         IDLE };
    setup(&r);
    const PortExtras input = r.e;
    run(&r, keys, 9);
    CHECK(shown(&r.fake, "< 2/3 >"));
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.nitro, input.nitro);
}

/* the left triangle turns back a page, from page 3 to page 2 and from page 2 to page 1, not on round to page 1 */
static void test_the_left_triangle_turns_back(void)
{
    Rig three;
    const int to_two[] = { PAGE_DOWN, PAGE_DOWN, AT(NAV_LEFT_X, BUTTONS_Y), CLICK, IDLE, IDLE, ESCAPE };
    setup(&three);
    run(&three, to_two, 7);
    CHECK(shown(&three.fake, "< 2/3 >"));
    CHECK(shown(&three.fake, "Speed units"));

    Rig two;
    const int to_one[] = { PAGE_DOWN, AT(NAV_LEFT_X, BUTTONS_Y), CLICK, IDLE, IDLE, ESCAPE };
    setup(&two);
    run(&two, to_one, 6);
    CHECK(shown(&two.fake, "1/3 >"));
    CHECK(shown(&two.fake, "Explode mode"));
}

int main(void)
{
    test_nothing_is_lit_with_the_pointer_on_nothing();
    test_the_row_under_the_pointer_is_lit();
    test_the_pointer_off_the_rows_selects_nothing();
    test_the_first_key_lights_the_first_row_only();
    test_a_click_acts_on_the_row_under_it();
    test_a_click_on_nothing_does_nothing();
    test_the_page_indicator_lights_alone();
    test_a_click_on_the_indicator_only_turns();
    test_the_left_triangle_turns_back();
    return TEST_REPORT();
}
