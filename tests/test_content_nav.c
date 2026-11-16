/* dep59 step 4b (T5): the CONTENT screen's selection (game/frontend/content_nav.h): the keys through the selector, the tabs, the list
 * (the All row, a row's box and arrows, the pages), the card's buttons and the foot; the clicks and the pointer on the mock-up's places;
 * what each asks the screen to do. */
#include "game/frontend/content_nav.h"
#include "test.h"

#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, DOWN = 0x5000, LEFT = 0x4b00, RIGHT = 0x4d00, PAGE_UP = 0x4900,
       PAGE_DOWN = 0x5100 };

static ContentRows rows;
static ContentView view;

/* `n` rows, the first `on` of them on and movable, all switchable; every card and foot button on */
static void fill(int n, int on, int locked)
{
    memset(&rows, 0, sizeof rows);
    rows.count = n, rows.locked = locked;
    for (int i = 0; i < n; i++) {
        rows.row[i].on = i < on;
        rows.row[i].switchable = !locked;
        rows.row[i].movable = !locked && i < on;
    }
    memset(&view, 0, sizeof view);
    view.rows = &rows;
    for (int i = 0; i < CONTENT_CARD_BUTTONS; i++) view.card_on[i] = 1;
    for (int i = 0; i < CONTENT_FOOT_BUTTONS; i++) view.foot_on[i] = 1;
}

static int line_y(int line) { return CONTENT_LIST_Y + line * CONTENT_LINE + 4; }

static void test_the_start_and_the_pages(void)
{
    ContentNav n;
    content_nav_start(&n);
    CHECK_EQ_INT(n.tab, CONTENT_CARS);
    CHECK_EQ_INT(n.at, CONTENT_AT_LIST);
    CHECK_EQ_INT(n.line, 1);
    CHECK_EQ_INT(n.item, CONTENT_ITEM_BOX);
    fill(0, 0, 0);
    CHECK_EQ_INT(content_nav_pages(&rows), 1);
    content_nav_clamp(&n, &view);
    CHECK_EQ_INT(n.line, 0);   /* no row: the All row */
    fill(8, 3, 0);
    CHECK_EQ_INT(content_nav_pages(&rows), 1);
    fill(9, 3, 0);
    CHECK_EQ_INT(content_nav_pages(&rows), 2);   /* 8 rows a page under the All row */
    fill(20, 3, 0);
    CHECK_EQ_INT(content_nav_pages(&rows), 3);
    n.page = 1;
    CHECK_EQ_INT(content_nav_row(&n, &rows, 0), -1);
    CHECK_EQ_INT(content_nav_row(&n, &rows, 1), 8);
    n.page = 2;
    CHECK_EQ_INT(content_nav_row(&n, &rows, 4), 19);
    CHECK_EQ_INT(content_nav_row(&n, &rows, 5), -2);   /* past the rows */
    n.line = 4;
    CHECK_EQ_INT(content_nav_selected_row(&n, &rows), 19);
    n.line = 6;
    content_nav_clamp(&n, &view);
    CHECK_EQ_INT(n.line, 4);   /* kept on the last row there is */
    n.page = 5;
    content_nav_clamp(&n, &view);
    CHECK_EQ_INT(n.page, 2);
}

static void test_up_and_down(void)
{
    ContentNav n;
    content_nav_start(&n);
    fill(3, 2, 0);
    CHECK_EQ_INT(content_nav_key(&n, &view, UP).kind, CONTENT_ACT_MOVED);
    CHECK_EQ_INT(n.line, 0);   /* the All row */
    content_nav_key(&n, &view, UP);
    CHECK_EQ_INT(n.at, CONTENT_AT_TABS);
    content_nav_key(&n, &view, UP);
    CHECK_EQ_INT(n.at, CONTENT_AT_SELECTOR);
    content_nav_key(&n, &view, UP);
    CHECK_EQ_INT(n.at, CONTENT_AT_FOOT);   /* round */
    content_nav_key(&n, &view, DOWN);
    CHECK_EQ_INT(n.at, CONTENT_AT_SELECTOR);
    content_nav_key(&n, &view, DOWN), content_nav_key(&n, &view, DOWN);
    CHECK_EQ_INT(n.at, CONTENT_AT_LIST);
    CHECK_EQ_INT(n.line, 0);
    for (int i = 0; i < 3; i++) content_nav_key(&n, &view, DOWN);
    CHECK_EQ_INT(n.line, 3);
    content_nav_key(&n, &view, DOWN);   /* past the last row: the foot */
    CHECK_EQ_INT(n.at, CONTENT_AT_FOOT);
    content_nav_key(&n, &view, UP);
    CHECK_EQ_INT(n.at, CONTENT_AT_LIST);
    CHECK_EQ_INT(n.line, 3);   /* the page's last row */
}

static void test_the_selector_and_the_tabs(void)
{
    ContentNav n;
    content_nav_start(&n);
    fill(3, 2, 0);
    n.at = CONTENT_AT_SELECTOR;
    ContentAction a = content_nav_key(&n, &view, RIGHT);
    CHECK_EQ_INT(a.kind, CONTENT_ACT_EDITION);
    CHECK_EQ_INT(a.arg, 1);
    a = content_nav_key(&n, &view, LEFT);
    CHECK_EQ_INT(a.arg, -1);
    CHECK_EQ_INT(content_nav_key(&n, &view, ENTER).arg, 1);
    n.at = CONTENT_AT_TABS;
    a = content_nav_key(&n, &view, RIGHT);
    CHECK_EQ_INT(a.kind, CONTENT_ACT_TAB);
    CHECK_EQ_INT(n.tab, CONTENT_TRACKS);
    content_nav_key(&n, &view, RIGHT);
    CHECK_EQ_INT(n.tab, CONTENT_TWEAKS);
    content_nav_key(&n, &view, RIGHT);
    CHECK_EQ_INT(n.tab, CONTENT_CARS);   /* round */
    content_nav_key(&n, &view, LEFT);
    CHECK_EQ_INT(n.tab, CONTENT_TWEAKS);
    n.page = 1, n.line = 3;
    content_nav_key(&n, &view, LEFT);
    CHECK_EQ_INT(n.page, 0);   /* another tab: its first page */
    CHECK_EQ_INT(n.line, 0);
}

static void test_a_row(void)
{
    ContentNav n;
    content_nav_start(&n);
    fill(3, 2, 0);
    ContentAction a = content_nav_key(&n, &view, ENTER);
    CHECK_EQ_INT(a.kind, CONTENT_ACT_SWITCH);
    CHECK_EQ_INT(a.row, 0);
    CHECK_EQ_INT(content_nav_key(&n, &view, RIGHT).kind, CONTENT_ACT_MOVED);
    CHECK_EQ_INT(n.item, CONTENT_ITEM_UP);
    a = content_nav_key(&n, &view, SPACE);
    CHECK_EQ_INT(a.kind, CONTENT_ACT_MOVE);
    CHECK_EQ_INT(a.arg, -1);
    content_nav_key(&n, &view, RIGHT);
    CHECK_EQ_INT(n.item, CONTENT_ITEM_DOWN);
    a = content_nav_key(&n, &view, ENTER);
    CHECK_EQ_INT(a.kind, CONTENT_ACT_MOVE);
    CHECK_EQ_INT(a.arg, 1);
    content_nav_key(&n, &view, RIGHT);   /* past the arrows: the card */
    CHECK_EQ_INT(n.at, CONTENT_AT_CARD);
    CHECK_EQ_INT(n.item, CONTENT_CARD_UPDATE);
    a = content_nav_key(&n, &view, ENTER);
    CHECK_EQ_INT(a.kind, CONTENT_ACT_CARD);
    CHECK_EQ_INT(a.arg, CONTENT_CARD_UPDATE);
    CHECK_EQ_INT(a.row, 0);
    content_nav_key(&n, &view, RIGHT);
    CHECK_EQ_INT(n.item, CONTENT_CARD_UNINSTALL);
    content_nav_key(&n, &view, DOWN);
    CHECK_EQ_INT(n.item, CONTENT_CARD_PREVIOUS);
    content_nav_key(&n, &view, LEFT);   /* back to the list, on the row it was for */
    CHECK_EQ_INT(n.at, CONTENT_AT_LIST);
    CHECK_EQ_INT(n.line, 1);
    CHECK_EQ_INT(n.item, CONTENT_ITEM_BOX);
    /* a row that is off has no arrows: Right goes to the card */
    content_nav_key(&n, &view, DOWN), content_nav_key(&n, &view, DOWN);
    CHECK_EQ_INT(n.line, 3);
    content_nav_key(&n, &view, RIGHT);
    CHECK_EQ_INT(n.at, CONTENT_AT_CARD);
    /* the All row: its box only, no card */
    n.at = CONTENT_AT_LIST, n.line = 0, n.item = CONTENT_ITEM_BOX;
    CHECK_EQ_INT(content_nav_key(&n, &view, ENTER).kind, CONTENT_ACT_ALL);
    CHECK_EQ_INT(content_nav_key(&n, &view, RIGHT).kind, CONTENT_ACT_NONE);
    CHECK_EQ_INT(n.at, CONTENT_AT_LIST);
}

/* the standard edition's rows: their boxes and arrows take nothing; the card and the foot still do */
static void test_a_locked_list(void)
{
    ContentNav n;
    content_nav_start(&n);
    fill(3, 3, 1);
    CHECK_EQ_INT(content_nav_key(&n, &view, ENTER).kind, CONTENT_ACT_NONE);
    content_nav_key(&n, &view, RIGHT);
    CHECK_EQ_INT(n.at, CONTENT_AT_CARD);   /* no arrows */
    CHECK_EQ_INT(content_nav_key(&n, &view, ENTER).kind, CONTENT_ACT_CARD);
    n.at = CONTENT_AT_LIST, n.line = 0, n.item = CONTENT_ITEM_BOX;
    CHECK_EQ_INT(content_nav_key(&n, &view, ENTER).kind, CONTENT_ACT_NONE);
    /* a button that is off is passed over by the keys and takes nothing */
    view.card_on[CONTENT_CARD_UPDATE] = view.card_on[CONTENT_CARD_UNINSTALL] = 0;
    n.line = 1;
    content_nav_key(&n, &view, RIGHT);
    CHECK_EQ_INT(n.item, CONTENT_CARD_PREVIOUS);
    view.card_on[CONTENT_CARD_PREVIOUS] = 0;
    n.at = CONTENT_AT_LIST;
    CHECK_EQ_INT(content_nav_key(&n, &view, RIGHT).kind, CONTENT_ACT_NONE);   /* no button on: stays */
    CHECK_EQ_INT(n.at, CONTENT_AT_LIST);
}

static void test_the_pages_by_keys(void)
{
    ContentNav n;
    content_nav_start(&n);
    fill(20, 3, 0);
    CHECK_EQ_INT(content_nav_key(&n, &view, PAGE_DOWN).kind, CONTENT_ACT_MOVED);
    CHECK_EQ_INT(n.page, 1);
    content_nav_key(&n, &view, PAGE_DOWN), content_nav_key(&n, &view, PAGE_DOWN);
    CHECK_EQ_INT(n.page, 2);   /* no further */
    CHECK_EQ_INT(n.line, 1);
    content_nav_key(&n, &view, PAGE_UP);
    CHECK_EQ_INT(n.page, 1);
    /* Down from a page's last line goes on to the next page's first row */
    n.line = 8;
    content_nav_key(&n, &view, DOWN);
    CHECK_EQ_INT(n.page, 2);
    CHECK_EQ_INT(n.line, 1);
    content_nav_key(&n, &view, UP);   /* and Up from its first row back */
    CHECK_EQ_INT(n.page, 1);
    CHECK_EQ_INT(n.line, 8);
}

static void test_the_foot(void)
{
    ContentNav n;
    content_nav_start(&n);
    fill(3, 2, 0);
    ContentAction a = content_nav_key(&n, &view, ESCAPE);
    CHECK_EQ_INT(a.kind, CONTENT_ACT_FOOT);
    CHECK_EQ_INT(a.arg, CONTENT_FOOT_DONE);
    n.at = CONTENT_AT_FOOT, n.item = CONTENT_FOOT_NEW;
    content_nav_key(&n, &view, RIGHT);
    CHECK_EQ_INT(n.item, CONTENT_FOOT_DUPLICATE);
    view.foot_on[CONTENT_FOOT_RENAME] = view.foot_on[CONTENT_FOOT_DELETE] = 0;   /* the standard edition's */
    content_nav_key(&n, &view, RIGHT);
    CHECK_EQ_INT(n.item, CONTENT_FOOT_INSTALL);
    content_nav_key(&n, &view, RIGHT);
    CHECK_EQ_INT(n.item, CONTENT_FOOT_DONE);
    content_nav_key(&n, &view, RIGHT);
    CHECK_EQ_INT(n.item, CONTENT_FOOT_DONE);   /* no further */
    a = content_nav_key(&n, &view, ENTER);
    CHECK_EQ_INT(a.kind, CONTENT_ACT_FOOT);
    CHECK_EQ_INT(a.arg, CONTENT_FOOT_DONE);
    n.item = CONTENT_FOOT_RENAME;   /* on one that is off: nothing */
    CHECK_EQ_INT(content_nav_key(&n, &view, ENTER).kind, CONTENT_ACT_NONE);
}

static void test_the_clicks(void)
{
    ContentNav n;
    content_nav_start(&n);
    fill(20, 3, 0);
    ContentAction a = content_nav_click(&n, &view, 600, CONTENT_SELECTOR_Y + 8);
    CHECK_EQ_INT(a.kind, CONTENT_ACT_EDITION);
    CHECK_EQ_INT(a.arg, 1);
    CHECK_EQ_INT(content_nav_click(&n, &view, 400, CONTENT_SELECTOR_Y + 8).arg, -1);
    a = content_nav_click(&n, &view, 130, CONTENT_TAB_Y + 8);
    CHECK_EQ_INT(a.kind, CONTENT_ACT_TAB);
    CHECK_EQ_INT(n.tab, CONTENT_TRACKS);
    CHECK_EQ_INT(content_nav_click(&n, &view, 130, CONTENT_TAB_Y + 8).kind, CONTENT_ACT_NONE);   /* the tab it is on */
    a = content_nav_click(&n, &view, 20, line_y(2));
    CHECK_EQ_INT(a.kind, CONTENT_ACT_SWITCH);
    CHECK_EQ_INT(a.row, 1);
    CHECK_EQ_INT(content_nav_click(&n, &view, 20, line_y(0)).kind, CONTENT_ACT_ALL);
    a = content_nav_click(&n, &view, 100, line_y(3));   /* the name: the row selected, nothing taken */
    CHECK_EQ_INT(a.kind, CONTENT_ACT_MOVED);
    CHECK_EQ_INT(n.line, 3);
    a = content_nav_click(&n, &view, CONTENT_ARROWS_X + 4, line_y(2));
    CHECK_EQ_INT(a.kind, CONTENT_ACT_MOVE);
    CHECK_EQ_INT(a.arg, -1);
    a = content_nav_click(&n, &view, CONTENT_DOWN_X + 4, line_y(2));
    CHECK_EQ_INT(a.kind, CONTENT_ACT_MOVE);
    CHECK_EQ_INT(a.arg, 1);
    CHECK_EQ_INT(content_nav_click(&n, &view, CONTENT_ARROWS_X + 4, line_y(5)).kind, CONTENT_ACT_MOVED);   /* off: no arrows */
    CHECK_EQ_INT(content_nav_click(&n, &view, CONTENT_PAGE_NEXT_X + 4, CONTENT_PAGE_Y + 6).kind, CONTENT_ACT_MOVED);
    CHECK_EQ_INT(n.page, 1);
    content_nav_click(&n, &view, CONTENT_PAGE_PREV_X + 4, CONTENT_PAGE_Y + 6);
    CHECK_EQ_INT(n.page, 0);
    n.line = 2;
    a = content_nav_click(&n, &view, CONTENT_UNINSTALL_X + 10, CONTENT_CARD_BUTTONS_Y + 6);
    CHECK_EQ_INT(a.kind, CONTENT_ACT_CARD);
    CHECK_EQ_INT(a.arg, CONTENT_CARD_UNINSTALL);
    CHECK_EQ_INT(a.row, 1);   /* the row the card shows */
    CHECK_EQ_INT(content_nav_click(&n, &view, 400, CONTENT_PREVIOUS_Y + 6).arg, CONTENT_CARD_PREVIOUS);
    view.card_on[CONTENT_CARD_PREVIOUS] = 0;
    CHECK_EQ_INT(content_nav_click(&n, &view, 400, CONTENT_PREVIOUS_Y + 6).kind, CONTENT_ACT_NONE);
    a = content_nav_click(&n, &view, 600, CONTENT_FOOT_Y + 6);
    CHECK_EQ_INT(a.kind, CONTENT_ACT_FOOT);
    CHECK_EQ_INT(a.arg, CONTENT_FOOT_DONE);
    CHECK_EQ_INT(content_nav_click(&n, &view, 90, CONTENT_FOOT_Y + 6).arg, CONTENT_FOOT_DUPLICATE);
    CHECK_EQ_INT(content_nav_click(&n, &view, 500, 300).kind, CONTENT_ACT_NONE);   /* the card's words */
    /* a line past the rows takes nothing */
    fill(2, 1, 0);
    n.page = 0;
    CHECK_EQ_INT(content_nav_click(&n, &view, 20, line_y(5)).kind, CONTENT_ACT_NONE);
}

static void test_the_pointer(void)
{
    ContentNav n;
    content_nav_start(&n);
    fill(5, 3, 0);
    CHECK_EQ_INT(content_nav_point(&n, &view, 100, line_y(3)).kind, CONTENT_ACT_MOVED);
    CHECK_EQ_INT(n.line, 3);
    CHECK_EQ_INT(content_nav_point(&n, &view, 100, line_y(3)).kind, CONTENT_ACT_NONE);   /* no change */
    content_nav_point(&n, &view, 600, CONTENT_FOOT_Y + 6);
    CHECK_EQ_INT(n.at, CONTENT_AT_FOOT);
    CHECK_EQ_INT(n.item, CONTENT_FOOT_DONE);
    CHECK_EQ_INT(content_nav_point(&n, &view, 500, 300).kind, CONTENT_ACT_NONE);   /* on nothing: kept */
    CHECK_EQ_INT(n.at, CONTENT_AT_FOOT);
}

int main(void)
{
    test_the_start_and_the_pages();
    test_up_and_down();
    test_the_selector_and_the_tabs();
    test_a_row();
    test_a_locked_list();
    test_the_pages_by_keys();
    test_the_foot();
    test_the_clicks();
    test_the_pointer();
    return TEST_REPORT();
}
