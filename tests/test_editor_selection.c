/* dep87: the control points a person selects in the track editor (editor/selection.h): a click, Shift's range, Cmd's toggle, a box,
 * the set kept in step when points come and go, the group moved and rotated, and the guard on removing them. */
#include "selection.h"
#include "test.h"

#include <math.h>
#include <string.h>

static int listed(const Selection *s, const char *want)
{
    char got[128];
    sel_describe(s, got, sizeof got);
    if (strcmp(got, want) != 0) printf("selection: '%s', expected '%s'\n", got, want);
    return strcmp(got, want) == 0;
}

static void test_clicks(void)
{
    Selection s;
    sel_init(&s);
    CHECK(sel_resize(&s, 16));
    CHECK_EQ_INT(sel_count(&s), 0);
    CHECK(listed(&s, ""));
    sel_click(&s, 3, SEL_ONE);
    CHECK(listed(&s, "3"));
    sel_click(&s, 7, SEL_RANGE);   /* Shift: 3 to 7 */
    CHECK(listed(&s, "3-7"));
    CHECK_EQ_INT(sel_count(&s), 5);
    CHECK_EQ_INT(sel_first(&s), 3);
    CHECK_EQ_INT(sel_last(&s), 7);
    sel_click(&s, 1, SEL_RANGE);   /* the range runs from the anchor (3), either way */
    CHECK(listed(&s, "1-3"));
    sel_click(&s, 9, SEL_TOGGLE);   /* Cmd adds one, and becomes the anchor */
    sel_click(&s, 14, SEL_TOGGLE);
    CHECK(listed(&s, "1-3, 9, 14"));
    sel_click(&s, 2, SEL_TOGGLE);   /* and takes one away */
    CHECK(listed(&s, "1, 3, 9, 14"));
    CHECK(sel_has(&s, 9) && !sel_has(&s, 2) && !sel_has(&s, 99) && !sel_has(&s, -1));
    sel_click(&s, 5, SEL_ONE);   /* a plain click: that one alone */
    CHECK(listed(&s, "5"));
    sel_click(&s, 40, SEL_ONE);   /* past the points: nothing changes */
    CHECK(listed(&s, "5"));
    sel_clear(&s);
    CHECK_EQ_INT(sel_count(&s), 0);
    CHECK_EQ_INT(sel_first(&s), -1);
    sel_click(&s, 4, SEL_RANGE);   /* Shift with no anchor: that one */
    CHECK(listed(&s, "4"));
    sel_free(&s);
}

static void test_box(void)
{
    const TracePoint p[6] = { { 0, 0 }, { 10, 0 }, { 20, 5 }, { 30, 10 }, { 40, 40 }, { 15, -5 } };
    Selection s;
    sel_init(&s);
    CHECK(sel_resize(&s, 6));
    sel_box(&s, p, 6, 35, 12, 5, -1, 0);   /* corners in any order: 1, 2 and 3 */
    CHECK(listed(&s, "1-3"));
    sel_box(&s, p, 6, 14, -10, 16, -4, 1);   /* adding (Shift): 5 too */
    CHECK(listed(&s, "1-3, 5"));
    sel_box(&s, p, 6, 100, 100, 200, 200, 0);   /* an empty box clears */
    CHECK_EQ_INT(sel_count(&s), 0);
    sel_free(&s);
}

static void test_points_come_and_go(void)
{
    Selection s;
    sel_init(&s);
    CHECK(sel_resize(&s, 10));
    sel_click(&s, 2, SEL_ONE);
    sel_click(&s, 4, SEL_RANGE);
    sel_click(&s, 8, SEL_TOGGLE);
    sel_inserted(&s, 3);   /* a point put in before 3: 3 and after move up one; the new one is not selected */
    CHECK_EQ_INT(sel_points(&s), 11);
    CHECK(listed(&s, "2, 4-5, 9"));
    sel_removed(&s, 4);   /* point 4 gone: it leaves the set, after it move down */
    CHECK_EQ_INT(sel_points(&s), 10);
    CHECK(listed(&s, "2, 4, 8"));
    sel_removed(&s, 0);
    CHECK(listed(&s, "1, 3, 7"));
    CHECK(sel_resize(&s, 4));   /* fewer points (an undo): the ones past the end go */
    CHECK(listed(&s, "1, 3"));
    CHECK(sel_resize(&s, 6));   /* more: the new ones are not selected */
    CHECK(listed(&s, "1, 3"));
    sel_free(&s);
}

static void test_describe_long(void)
{
    Selection s;
    sel_init(&s);
    CHECK(sel_resize(&s, 200));
    for (int i = 0; i < 200; i += 2) sel_click(&s, i, SEL_TOGGLE);
    char text[24];
    sel_describe(&s, text, sizeof text);   /* cut to the room, ending in "..." */
    CHECK(strlen(text) < sizeof text && strcmp(text + strlen(text) - 3, "...") == 0);
    sel_free(&s);
}

static void test_move_and_rotate(void)
{
    TracePoint p[5] = { { 0, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 }, { 50, 50 } };
    Selection s;
    sel_init(&s);
    CHECK(sel_resize(&s, 5));
    sel_click(&s, 1, SEL_ONE);
    sel_click(&s, 3, SEL_RANGE);
    sel_move(&s, p, 5, 2.5, -1);   /* only the selected move */
    CHECK(p[0].x == 0 && p[0].z == 0 && p[4].x == 50);
    CHECK(p[1].x == 12.5 && p[1].z == -1 && p[3].x == 2.5 && p[3].z == 9);
    sel_move(&s, p, 5, -2.5, 1);
    const TracePoint from[5] = { p[0], p[1], p[2], p[3], p[4] };
    sel_rotate(&s, p, 5, from, 90);   /* about the first selected (1: 10,0), clockwise on screen (z up): up becomes right */
    CHECK(fabs(p[1].x - 10) < 1e-9 && fabs(p[1].z - 0) < 1e-9);
    CHECK(fabs(p[2].x - 20) < 1e-9 && fabs(p[2].z - 0) < 1e-9);   /* (0,+10) from 1 turns to (+10,0) */
    CHECK(fabs(p[3].x - 20) < 1e-9 && fabs(p[3].z - 10) < 1e-9);   /* (-10,+10) turns to (+10,+10) */
    CHECK(p[0].x == 0 && p[4].x == 50);
    sel_rotate(&s, p, 5, from, 0);   /* from the same start: back where they were (a live field rotates from where it began) */
    CHECK(fabs(p[2].x - 10) < 1e-9 && fabs(p[2].z - 10) < 1e-9 && fabs(p[3].x) < 1e-9);
    sel_free(&s);
}

static void test_remove_guard(void)
{
    Selection s;
    sel_init(&s);
    CHECK(sel_resize(&s, 5));
    char why[128] = "";
    CHECK(!sel_can_remove(&s, why, sizeof why));   /* nothing selected */
    CHECK(strstr(why, "select") != NULL);
    sel_click(&s, 0, SEL_ONE);
    sel_click(&s, 2, SEL_RANGE);
    CHECK(sel_can_remove(&s, why, sizeof why));   /* 3 of 5: 2 stay */
    sel_click(&s, 3, SEL_TOGGLE);
    CHECK(!sel_can_remove(&s, why, sizeof why));   /* 4 of 5: 1 would stay */
    CHECK(strstr(why, "at least 2 points") != NULL && strstr(why, "4 of 5") != NULL);
    sel_free(&s);
}

int main(void)
{
    test_clicks();
    test_box();
    test_points_come_and_go();
    test_describe_long();
    test_move_and_rotate();
    test_remove_guard();
    return TEST_REPORT();
}
