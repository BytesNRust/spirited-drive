/* See content_nav.h. */
#include "game/frontend/content_nav.h"
#include "game/sys/page_keys.h"

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, DOWN = 0x5000, LEFT = 0x4b00, RIGHT = 0x4d00, PAGE_UP = 0x4900,
       PAGE_DOWN = 0x5100 };
enum { PER_PAGE = CONTENT_LINES - 1 };   /* the rows a page shows under its All row */

/* the tabs' and the foot's buttons' spans, x0 .. x1 (x1 left out) */
static const int TAB_X[CONTENT_TABS + 1] = { 14, 143, 250, CONTENT_LIST_X1 };
static ContentAction act(ContentActionKind kind, int arg, int row) { return (ContentAction){ kind, arg, row }; }
static ContentAction none(void) { return act(CONTENT_ACT_NONE, 0, 0); }
static ContentAction moved(void) { return act(CONTENT_ACT_MOVED, 0, 0); }

void content_nav_start(ContentNav *n)
{
    *n = (ContentNav){ CONTENT_CARS, CONTENT_AT_LIST, 1, CONTENT_ITEM_BOX, 0 };
}

int content_nav_pages(const ContentRows *rows)
{
    return rows->count <= PER_PAGE ? 1 : (rows->count + PER_PAGE - 1) / PER_PAGE;
}

int content_nav_row(const ContentNav *n, const ContentRows *rows, int line)
{
    if (line == 0) return -1;
    const int row = n->page * PER_PAGE + line - 1;
    return line > 0 && line < CONTENT_LINES && row < rows->count ? row : -2;
}

int content_nav_selected_row(const ContentNav *n, const ContentRows *rows)
{
    return n->at == CONTENT_AT_LIST || n->at == CONTENT_AT_CARD ? content_nav_row(n, rows, n->line) : -2;
}

/* the page's last line that shows a row (0, the All row, when it shows none) */
static int last_line(const ContentNav *n, const ContentRows *rows)
{
    int line = CONTENT_LINES - 1;
    while (line > 0 && content_nav_row(n, rows, line) == -2) line--;
    return line;
}

void content_nav_clamp(ContentNav *n, const ContentView *v)
{
    const int pages = content_nav_pages(v->rows);
    if (n->page >= pages) n->page = pages - 1;
    if (n->page < 0) n->page = 0;
    if (n->line > last_line(n, v->rows)) n->line = last_line(n, v->rows);
    if (n->at == CONTENT_AT_LIST && n->item != CONTENT_ITEM_BOX) {
        const int row = content_nav_row(n, v->rows, n->line);
        if (row < 0 || !v->rows->row[row].movable) n->item = CONTENT_ITEM_BOX;
    }
}

/* the card's first button that is on, from `from` stepping by `step`; -1 for none */
static int card_from(const ContentView *v, int from, int step)
{
    for (int b = from; b >= 0 && b < CONTENT_CARD_BUTTONS; b += step)
        if (v->card_on[b]) return b;
    return -1;
}

static int foot_from(const ContentView *v, int from, int step)
{
    for (int b = from; b >= 0 && b < CONTENT_FOOT_BUTTONS; b += step)
        if (v->foot_on[b]) return b;
    return -1;
}

static int row_movable(const ContentNav *n, const ContentView *v)
{
    const int row = content_nav_row(n, v->rows, n->line);
    return row >= 0 && v->rows->row[row].movable;
}

static ContentAction go(ContentNav *n, int at, int line, int item)
{
    n->at = at, n->line = line, n->item = item;
    return moved();
}

static ContentAction list_up(ContentNav *n, const ContentView *v)
{
    if (n->line > 1 || (n->line == 1 && n->page == 0)) return go(n, CONTENT_AT_LIST, n->line - 1, CONTENT_ITEM_BOX);
    if (n->line == 1) {
        n->page--;
        return go(n, CONTENT_AT_LIST, last_line(n, v->rows), CONTENT_ITEM_BOX);
    }
    return go(n, CONTENT_AT_TABS, n->line, n->item);
}

static ContentAction list_down(ContentNav *n, const ContentView *v)
{
    if (n->line < last_line(n, v->rows)) return go(n, CONTENT_AT_LIST, n->line + 1, CONTENT_ITEM_BOX);
    if (n->page + 1 < content_nav_pages(v->rows)) {
        n->page++;
        return go(n, CONTENT_AT_LIST, 1, CONTENT_ITEM_BOX);
    }
    const int b = foot_from(v, CONTENT_FOOT_DONE, -1);
    return go(n, CONTENT_AT_FOOT, n->line, b >= 0 ? b : CONTENT_FOOT_DONE);
}

static ContentAction vertical(ContentNav *n, const ContentView *v, int down)
{
    switch (n->at) {
    case CONTENT_AT_SELECTOR:
        return down ? go(n, CONTENT_AT_TABS, n->line, n->item) : go(n, CONTENT_AT_FOOT, n->line, CONTENT_FOOT_DONE);
    case CONTENT_AT_TABS:
        if (!down) return go(n, CONTENT_AT_SELECTOR, n->line, n->item);
        n->page = 0;
        return go(n, CONTENT_AT_LIST, 0, CONTENT_ITEM_BOX);
    case CONTENT_AT_LIST: return down ? list_down(n, v) : list_up(n, v);
    case CONTENT_AT_CARD:
        if (down && n->item != CONTENT_CARD_PREVIOUS && v->card_on[CONTENT_CARD_PREVIOUS]) return go(n, CONTENT_AT_CARD, n->line, CONTENT_CARD_PREVIOUS);
        if (down) return go(n, CONTENT_AT_FOOT, n->line, CONTENT_FOOT_DONE);
        if (n->item == CONTENT_CARD_PREVIOUS && card_from(v, CONTENT_CARD_UPDATE, 1) != CONTENT_CARD_PREVIOUS)
            return go(n, CONTENT_AT_CARD, n->line, card_from(v, CONTENT_CARD_UPDATE, 1));
        return go(n, CONTENT_AT_LIST, n->line, CONTENT_ITEM_BOX);
    default:
        if (down) return go(n, CONTENT_AT_SELECTOR, n->line, n->item);
        return go(n, CONTENT_AT_LIST, last_line(n, v->rows), CONTENT_ITEM_BOX);
    }
}

static ContentAction tab_to(ContentNav *n, int tab)
{
    n->tab = tab, n->page = 0, n->line = 0, n->item = CONTENT_ITEM_BOX;
    return act(CONTENT_ACT_TAB, tab, 0);
}

/* the pad's shoulder buttons: the previous or the next tab wherever the selection is (none past the first or the last); on the card it moves to the list */
static ContentAction tab_step(ContentNav *n, int step)
{
    const int tab = n->tab + step;
    if (tab < 0 || tab >= CONTENT_TABS) return none();
    const int at = n->at, item = n->item;
    const ContentAction r = tab_to(n, tab);
    if (at == CONTENT_AT_CARD) n->at = CONTENT_AT_LIST;   /* the card of the old tab's row is gone */
    else if (at != CONTENT_AT_LIST) n->at = at, n->item = item;   /* the selector, the tabs, the foot: where they were */
    return r;
}

/* Left / Right in the list: along the row's box and arrows, then onto the card */
static ContentAction list_side(ContentNav *n, const ContentView *v, int step)
{
    const int movable = row_movable(n, v), row = content_nav_row(n, v->rows, n->line);
    if (step < 0) return n->item > CONTENT_ITEM_BOX ? go(n, CONTENT_AT_LIST, n->line, n->item - 1) : none();
    if (movable && n->item < CONTENT_ITEM_DOWN) return go(n, CONTENT_AT_LIST, n->line, n->item + 1);
    const int b = row >= 0 ? card_from(v, CONTENT_CARD_UPDATE, 1) : -1;
    return b >= 0 ? go(n, CONTENT_AT_CARD, n->line, b) : none();
}

static ContentAction card_side(ContentNav *n, const ContentView *v, int step)
{
    if (n->item == CONTENT_CARD_UNINSTALL && step < 0 && v->card_on[CONTENT_CARD_UPDATE]) return go(n, CONTENT_AT_CARD, n->line, CONTENT_CARD_UPDATE);
    if (n->item == CONTENT_CARD_UPDATE && step > 0 && v->card_on[CONTENT_CARD_UNINSTALL]) return go(n, CONTENT_AT_CARD, n->line, CONTENT_CARD_UNINSTALL);
    if (step < 0) return go(n, CONTENT_AT_LIST, n->line, CONTENT_ITEM_BOX);
    return none();
}

static ContentAction side(ContentNav *n, const ContentView *v, int step)
{
    switch (n->at) {
    case CONTENT_AT_SELECTOR: return act(CONTENT_ACT_EDITION, step, 0);
    case CONTENT_AT_TABS: return tab_to(n, (n->tab + step + CONTENT_TABS) % CONTENT_TABS);
    case CONTENT_AT_LIST: return list_side(n, v, step);
    case CONTENT_AT_CARD: return card_side(n, v, step);
    default: {
        const int b = foot_from(v, n->item + step, step);
        return b >= 0 ? go(n, CONTENT_AT_FOOT, n->line, b) : none();
    }
    }
}

/* the list's item under the selection taken */
static ContentAction take_list(const ContentNav *n, const ContentView *v)
{
    const int row = content_nav_row(n, v->rows, n->line);
    if (row == -1) return v->rows->locked ? none() : act(CONTENT_ACT_ALL, 0, 0);
    if (row < 0) return none();
    const ContentRow *r = &v->rows->row[row];
    if (n->item == CONTENT_ITEM_BOX) return r->switchable ? act(CONTENT_ACT_SWITCH, 0, row) : none();
    return r->movable ? act(CONTENT_ACT_MOVE, n->item == CONTENT_ITEM_UP ? -1 : 1, row) : none();
}

static ContentAction take(const ContentNav *n, const ContentView *v)
{
    switch (n->at) {
    case CONTENT_AT_SELECTOR: return act(CONTENT_ACT_EDITION, 1, 0);
    case CONTENT_AT_TABS: return none();
    case CONTENT_AT_LIST: return take_list(n, v);
    case CONTENT_AT_CARD: {
        const int row = content_nav_row(n, v->rows, n->line);
        return row >= 0 && v->card_on[n->item] ? act(CONTENT_ACT_CARD, n->item, row) : none();
    }
    default: return v->foot_on[n->item] ? act(CONTENT_ACT_FOOT, n->item, 0) : none();
    }
}

static ContentAction page_to(ContentNav *n, const ContentView *v, int page)
{
    const int pages = content_nav_pages(v->rows);
    page = (page % pages + pages) % pages;   /* the pager wraps, as the CD's */
    if (page == n->page) return none();
    n->page = page;
    content_nav_clamp(n, v);
    return moved();
}

ContentAction content_nav_key(ContentNav *n, const ContentView *v, int key)
{
    switch (key) {
    case UP: return vertical(n, v, 0);
    case DOWN: return vertical(n, v, 1);
    case LEFT: return side(n, v, -1);
    case RIGHT: return side(n, v, 1);
    case ENTER:
    case SPACE: return take(n, v);
    case ESCAPE: return act(CONTENT_ACT_FOOT, CONTENT_FOOT_DONE, 0);
    case TAB_KEY_PREVIOUS: return tab_step(n, -1);
    case TAB_KEY_NEXT: return tab_step(n, 1);
    case PAGE_UP: return page_to(n, v, n->page - 1);
    case PAGE_DOWN: return page_to(n, v, n->page + 1);
    default: return none();
    }
}

/* what is at (x, y): the selection it would be (`at`, `line`, `item`); 0 on nothing. A tab gives its number as `item`, the selector its
 * step, the page indicator CONTENT_AT_LIST with line -1 and its step as `item` */
typedef struct {
    int at, line, item;
} Spot;

static int within(int v, int lo, int hi) { return v >= lo && v < hi; }

static int list_spot(const ContentNav *n, const ContentView *v, int x, int y, Spot *s)
{
    if (within(y, CONTENT_PAGE_Y, CONTENT_PAGE_Y + CONTENT_TEXT_H)) {
        if (within(x, CONTENT_PAGE_NEXT_X, CONTENT_PAGE_NEXT_X + 50)) return *s = (Spot){ CONTENT_AT_LIST, -1, 1 }, 1;
        if (within(x, CONTENT_PAGE_PREV_X, CONTENT_PAGE_NEXT_X)) return *s = (Spot){ CONTENT_AT_LIST, -1, -1 }, 1;
        return 0;
    }
    if (!within(x, CONTENT_LIST_X0, CONTENT_LIST_X1) || y < CONTENT_LIST_Y || (y - CONTENT_LIST_Y) % CONTENT_LINE >= CONTENT_TEXT_H) return 0;
    const int line = (y - CONTENT_LIST_Y) / CONTENT_LINE, row = content_nav_row(n, v->rows, line);
    if (line >= CONTENT_LINES || row == -2) return 0;
    int item = CONTENT_ITEM_BOX;
    if (x >= CONTENT_NAME_X) item = -1;   /* the name: the row selected only */
    if (row >= 0 && v->rows->row[row].movable && within(x, CONTENT_ARROWS_X, CONTENT_DOWN_X)) item = CONTENT_ITEM_UP;
    if (row >= 0 && v->rows->row[row].movable && within(x, CONTENT_DOWN_X, CONTENT_DOWN_X + 20)) item = CONTENT_ITEM_DOWN;
    *s = (Spot){ CONTENT_AT_LIST, line, item };
    return 1;
}

static int spot_at(const ContentNav *n, const ContentView *v, int x, int y, Spot *s)
{
    if (within(y, CONTENT_SELECTOR_Y, CONTENT_SELECTOR_Y + CONTENT_TEXT_H) && x >= CONTENT_SELECTOR_LEFT)
        return *s = (Spot){ CONTENT_AT_SELECTOR, n->line, x < CONTENT_SELECTOR_X ? -1 : 1 }, 1;
    if (within(y, CONTENT_TAB_Y, CONTENT_TAB_Y + CONTENT_TEXT_H))
        for (int t = 0; t < CONTENT_TABS; t++)
            if (within(x, TAB_X[t], TAB_X[t + 1])) return *s = (Spot){ CONTENT_AT_TABS, n->line, t }, 1;
    if (within(y, CONTENT_CARD_BUTTONS_Y, CONTENT_CARD_BUTTONS_Y + CONTENT_TEXT_H)) {
        if (within(x, CONTENT_CARD_X0, CONTENT_CARD_X0 + 100)) return *s = (Spot){ CONTENT_AT_CARD, n->line, CONTENT_CARD_UPDATE }, 1;
        if (within(x, CONTENT_UNINSTALL_X, CONTENT_CARD_X1)) return *s = (Spot){ CONTENT_AT_CARD, n->line, CONTENT_CARD_UNINSTALL }, 1;
    }
    if (within(y, CONTENT_PREVIOUS_Y, CONTENT_PREVIOUS_Y + CONTENT_TEXT_H) && within(x, CONTENT_CARD_X0, CONTENT_CARD_X1))
        return *s = (Spot){ CONTENT_AT_CARD, n->line, CONTENT_CARD_PREVIOUS }, 1;
    if (within(y, CONTENT_FOOT_Y, CONTENT_FOOT_Y + CONTENT_TEXT_H))
        for (int b = 0; b < CONTENT_FOOT_BUTTONS; b++)
            if (within(x, v->foot_x[b][0], v->foot_x[b][1])) return *s = (Spot){ CONTENT_AT_FOOT, n->line, b }, 1;
    return list_spot(n, v, x, y, s);
}

ContentAction content_nav_click(ContentNav *n, const ContentView *v, int x, int y)
{
    Spot s;
    if (!spot_at(n, v, x, y, &s)) return none();
    if (s.at == CONTENT_AT_SELECTOR) return n->at = s.at, act(CONTENT_ACT_EDITION, s.item, 0);
    if (s.at == CONTENT_AT_TABS) return n->at = s.at, s.item == n->tab ? none() : tab_to(n, s.item);
    if (s.at == CONTENT_AT_LIST && s.line < 0) return page_to(n, v, n->page + s.item);
    if (s.at == CONTENT_AT_CARD && (content_nav_row(n, v->rows, n->line) < 0 || !v->card_on[s.item])) return none();
    if (s.at == CONTENT_AT_FOOT && !v->foot_on[s.item]) return none();
    go(n, s.at, s.line, s.item < 0 ? CONTENT_ITEM_BOX : s.item);
    return s.item < 0 ? moved() : take(n, v);
}

ContentAction content_nav_point(ContentNav *n, const ContentView *v, int x, int y)
{
    Spot s;
    if (!spot_at(n, v, x, y, &s) || s.at == CONTENT_AT_SELECTOR || s.at == CONTENT_AT_TABS || (s.at == CONTENT_AT_LIST && s.line < 0))
        return none();
    if (s.item < 0) s.item = CONTENT_ITEM_BOX;
    if (s.at == n->at && s.line == n->line && s.item == n->item) return none();
    return go(n, s.at, s.line, s.item);
}
