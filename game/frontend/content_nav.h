/* port (dep59 step 4b, T5): the CONTENT screen's places and its selection, with no drawing (content_screen.c draws it): the owner's
 * approved mock-up (mockups/content_sheet.png) in the game's 640x480 pixels, text y the words' tops.
 *   top     the title (Customize Game's place) and, at the right, Game Mode's selector of the editions "<  name  >" centred at x 436
 *   tabs    Cars | Tracks | Tweaks at y 102, a rule under them at y 126 across the list's width
 *   list    x 14..372: the All row, then the tab's rows, CONTENT_LINE apart from y 136 down to above the standard edition's note; the
 *           page indicator under them. A row: its box at x 14, the name at 56, the version right-aligned at 292, the arrows at 306
 *   card    x 392..626 right of a rule at x 380: the preview, the name, version and author, two lines of description, "Used in", then
 *           Update..., Uninstall and Previous version
 *   foot    a rule at y 434; New, Duplicate, Rename, Delete, Install... at the left and Done right-aligned at 616, at y 446
 * The selection walks them with the keys: Up / Down through the selector, the tabs, the list's lines and the foot; Left / Right along a
 * line (the selector's editions, the tabs, a row's box and arrows then the card's buttons, the foot's buttons); Page Up / Page Down turn
 * the list's pages; Enter or Space takes the item; Escape is Done. A click takes what is under the pointer, a move lights it. Each
 * returns what the screen is to do (ContentAction); the screen does it and tells the selection what it now shows (ContentView). */
#ifndef TNFS_CONTENT_NAV_H
#define TNFS_CONTENT_NAV_H

#include "game/frontend/content_rows.h"

enum {
    CONTENT_TITLE_X = 14, CONTENT_TITLE_Y = 62,
    CONTENT_SELECTOR_X = 436, CONTENT_SELECTOR_Y = 62,
    CONTENT_TAB_Y = 102, CONTENT_TAB_RULE_Y = 126,
    CONTENT_LIST_X0 = 14, CONTENT_LIST_X1 = 372, CONTENT_LIST_Y = 136, CONTENT_LINE = 26,
    CONTENT_BOX_X = 14, CONTENT_NAME_X = 56, CONTENT_VERSION_RIGHT = 292, CONTENT_ARROWS_X = 306, CONTENT_DOWN_X = 330,
    CONTENT_LINES = 9,                       /* the All row and 8 rows a page: the last ends above the page indicator's line */
    CONTENT_PAGE_Y = CONTENT_LIST_Y + CONTENT_LINES * CONTENT_LINE, CONTENT_PAGE_X = 290, CONTENT_PAGE_NEXT_X = 330,
    CONTENT_PAGE_PREV_X = 268,
    CONTENT_SPLIT_X = 380, CONTENT_SPLIT_Y0 = 104,
    CONTENT_CARD_X0 = 392, CONTENT_CARD_X1 = 626, CONTENT_PREVIEW_Y0 = 136, CONTENT_PREVIEW_Y1 = 222,
    CONTENT_CARD_NAME_Y = 232, CONTENT_CARD_BY_Y = 262, CONTENT_CARD_TEXT_Y = 286, CONTENT_CARD_TEXT_LINE = 18,
    CONTENT_CARD_USED_Y = 332, CONTENT_CARD_BUTTONS_Y = 358, CONTENT_UNINSTALL_X = 500, CONTENT_PREVIOUS_Y = 386,
    /* the card's translucent panel (the gallery's): the preview, the words and the buttons on it, clear of the split and the note */
    CONTENT_CARD_PANEL_X0 = 386, CONTENT_CARD_PANEL_X1 = 632, CONTENT_CARD_PANEL_Y0 = 130, CONTENT_CARD_PANEL_Y1 = 408,
    CONTENT_NOTE_X = 14, CONTENT_NOTE_Y = 412,
    CONTENT_FOOT_RULE_Y = 434, CONTENT_FOOT_Y = 446, CONTENT_DONE_RIGHT = 616,
    CONTENT_TEXT_H = 20                      /* a line's height for the pointer */
};

/* the selection's lines, top to bottom */
enum { CONTENT_AT_SELECTOR, CONTENT_AT_TABS, CONTENT_AT_LIST, CONTENT_AT_CARD, CONTENT_AT_FOOT };
/* a list row's items, left to right (the All row has its box only) */
enum { CONTENT_ITEM_BOX, CONTENT_ITEM_UP, CONTENT_ITEM_DOWN };
/* the card's buttons and the foot's, left to right */
enum { CONTENT_CARD_UPDATE, CONTENT_CARD_UNINSTALL, CONTENT_CARD_PREVIOUS, CONTENT_CARD_BUTTONS };
enum { CONTENT_FOOT_NEW, CONTENT_FOOT_DUPLICATE, CONTENT_FOOT_RENAME, CONTENT_FOOT_DELETE, CONTENT_FOOT_INSTALL, CONTENT_FOOT_DONE,
       CONTENT_FOOT_BUTTONS };

/* what the screen is to do */
typedef enum {
    CONTENT_ACT_NONE,      /* nothing (a key that does nothing here) */
    CONTENT_ACT_MOVED,     /* the selection moved: draw again */
    CONTENT_ACT_EDITION,   /* the selector's next (arg 1) or previous (-1) edition */
    CONTENT_ACT_TAB,       /* the tab changed (the state's tab is the new one): build its rows */
    CONTENT_ACT_SWITCH,    /* row `row`'s box */
    CONTENT_ACT_ALL,       /* the All row's box */
    CONTENT_ACT_MOVE,      /* row `row` up (arg -1) or down (1) */
    CONTENT_ACT_CARD,      /* the card's button `arg` (CONTENT_CARD_*) for row `row` */
    CONTENT_ACT_FOOT,      /* the foot's button `arg` (CONTENT_FOOT_*): Done, and Escape, too */
} ContentActionKind;

typedef struct {
    ContentActionKind kind;
    int arg;
    int row;   /* a row of the tab's ContentRows */
} ContentAction;

/* what is on screen, for the selection: the tab's rows, and which card and foot buttons can be taken */
typedef struct {
    const ContentRows *rows;
    int card_on[CONTENT_CARD_BUTTONS];
    int foot_on[CONTENT_FOOT_BUTTONS];
} ContentView;

typedef struct {
    int tab;     /* CONTENT_CARS .. */
    int at;      /* CONTENT_AT_* */
    int line;    /* in the list: 0 the All row, k the page's k-th row */
    int item;    /* in the list: CONTENT_ITEM_*; on the card: CONTENT_CARD_*; in the foot: CONTENT_FOOT_* */
    int page;
} ContentNav;

/* the selection as the screen opens: the list's first row of the Cars tab (the All row when there is none) */
void content_nav_start(ContentNav *n);
/* how many pages the rows take, at least 1 */
int content_nav_pages(const ContentRows *rows);
/* the row (of the tab's ContentRows) the list's line `line` shows on page `page`; -1 the All row, -2 none */
int content_nav_row(const ContentNav *n, const ContentRows *rows, int line);
/* the row the selection is on (-1 the All row, -2 none: not in the list or past the rows) */
int content_nav_selected_row(const ContentNav *n, const ContentRows *rows);
/* the rows rebuilt (another tab, an edit, another edition): the selection kept on a line and page that still exist */
void content_nav_clamp(ContentNav *n, const ContentView *v);

ContentAction content_nav_key(ContentNav *n, const ContentView *v, int key);
/* a click at (x, y): the selection moved onto what is there and taken; NONE on nothing */
ContentAction content_nav_click(ContentNav *n, const ContentView *v, int x, int y);
/* the pointer at (x, y): the selection on what is there (MOVED when it changed), NONE on nothing */
ContentAction content_nav_point(ContentNav *n, const ContentView *v, int x, int y);

#endif
