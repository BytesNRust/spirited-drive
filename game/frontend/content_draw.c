/* See content_draw.h. */
#include "game/frontend/content_draw.h"

#include "game/frontend/text_wrap.h"
#include "game/frontend/ui_kit.h"
#include "game/sys/port_log.h"

#include <stdio.h>
#include <string.h>

/* the sizes, num/den of the font's: GRAVER18 is wide, so the list's words and the tabs (3/4, as Control Central's card names) are
 * smaller than the mock-up's Mac font drew them, the versions and the card's words smaller still (3/5) */
enum { SMALL_NUM = 3, SMALL_DEN = 5, BUTTON_NUM = 4, BUTTON_DEN = 5, ROW_NUM = 3, ROW_DEN = 4, TAB_NUM = 3, TAB_DEN = 4, GAP = 8, ARROW_DY = 3,
       VERSION_DY = 2, CARD_LINES = 2,
       DONE_NUM = 11, DONE_DEN = 12, DONE_DY = 1 };   /* Done's letters: the main face's capitals are 12 rows at 1/1, the CD's DONE 11 */

static const char *const TAB_NAME[CONTENT_TABS] = { "Cars", "Tracks", "Tweaks" };
static const int TAB_CENTRE[CONTENT_TABS] = { 90, 197, 304 };   /* centred over the list, fixed places */
static const char *const CARD_WORDS[CONTENT_CARD_BUTTONS] = { "Update...", "Uninstall", "Previous version" };
static const int CARD_X[CONTENT_CARD_BUTTONS] = { CONTENT_CARD_X0, CONTENT_UNINSTALL_X, CONTENT_CARD_X0 };
static const int CARD_Y[CONTENT_CARD_BUTTONS] = { CONTENT_CARD_BUTTONS_Y, CONTENT_CARD_BUTTONS_Y, CONTENT_PREVIOUS_Y };
static const char *const FOOT_WORDS[CONTENT_FOOT_BUTTONS] = { "New", "Duplicate", "Rename", "Delete", "Install...", "Done" };
static const int FOOT_X[CONTENT_FOOT_BUTTONS - 1] = { 14, 66, 170, 252, 420 };
/* the selector's name: the main face's size, or smaller when the name is wide (ui_fit shrinks it, and cuts it at the smallest); its note: cut */
static const UiSize NAME_SIZES[] = { { 1, 1 }, { 5, 6 }, { 3, 4 } };
static const UiSize NOTE_SIZES[] = { { SMALL_NUM, SMALL_DEN } };
enum { NAME_SIZE_COUNT = (int)(sizeof NAME_SIZES / sizeof NAME_SIZES[0]), NAME_ROWS = 12 };   /* the capitals' rows at 1/1 */

static int measure(void *ctx, const char *text, int num, int den)
{
    const ContentOps *const o = ctx;
    return o->text_width(o->ctx, text, num, den);
}

static const char READ_ONLY[] = "Read only. Duplicate to edit.";
enum { YELLOW_RGB = 0xffce06, UNDERLINE_DY = 4 };

/* `text` cut with "..." to at most `width` pixels at num/den, into `out` */
static const char *fit(const ContentOps *o, const char *text, int num, int den, int width, char *out, size_t cap)
{
    snprintf(out, cap, "%s", text);
    for (size_t n = strlen(out); n > 0 && o->text_width(o->ctx, out, num, den) > width; n--) snprintf(out, cap, "%.*s...", (int)n - 1, text);
    return out;
}

/* a mod's or an edition's own words as the main face draws them right (text_words), into `out` */
static const char *clean(const char *text, char *out, int cap)
{
    text_words(text, out, cap);
    return out;
}

static void rule(const ContentOps *o, int x0, int y0, int x1, int y1) { o->fill(o->ctx, x0, y0, x1, y1, CONTENT_RULE_RGB); }

/* a button's style: grey when it cannot be taken, yellow under the selection */
static int button_style(int on, int lit) { return !on ? CONTENT_DIM : lit ? CONTENT_YELLOW : CONTENT_CREAM; }

static void top(const ContentOps *o, const ContentPage *p)
{
    char name[64], plain[64];
    o->title(o->ctx, "Content", CONTENT_TITLE_X, CONTENT_TITLE_Y);
    const int sel = p->nav->at == CONTENT_AT_SELECTOR && !p->quiet;
    UiSize name_size = NAME_SIZES[0], note_size = NOTE_SIZES[0];
    const int has_name = ui_fit(clean(p->edition, plain, sizeof plain), UI_SELECTOR_TEXT_W, UI_FIT_SHRINK, NAME_SIZES, NAME_SIZE_COUNT, measure, (void *)o, name,
                                sizeof name, &name_size);
    char note[64];
    const int has_note = p->view->rows->locked &&   /* the standard edition only: an edition of the player's own has no line under its name */
                         ui_fit(READ_ONLY, UI_SELECTOR_TEXT_W, UI_FIT_CUT, NOTE_SIZES, 1, measure, (void *)o, note, sizeof note, &note_size);
    const int name_w = has_name ? o->text_width(o->ctx, name, name_size.num, name_size.den) : 0;
    const int note_w = has_note ? o->text_width(o->ctx, note, note_size.num, note_size.den) : 0;
    const UiSelectorLayout l = ui_selector_layout(CONTENT_SELECTOR_X, name_w > note_w ? name_w : note_w);   /* the arrows outside the widest line */
    o->arrow(o->ctx, CONTENT_ARROW_LEFT, sel, l.left_x, CONTENT_SELECTOR_Y + ARROW_DY);
    o->arrow(o->ctx, CONTENT_ARROW_RIGHT, sel, l.right_x, CONTENT_SELECTOR_Y + ARROW_DY);
    if (has_name)   /* a smaller name sits in the middle of the full size's rows */
        o->text(o->ctx, sel ? CONTENT_YELLOW : CONTENT_CREAM, name, CONTENT_SELECTOR_X - name_w / 2,
                CONTENT_SELECTOR_Y + NAME_ROWS * (name_size.den - name_size.num) / (2 * name_size.den), name_size.num, name_size.den);
    if (has_note)
        o->text(o->ctx, p->flash ? CONTENT_YELLOW : CONTENT_DIM, note, CONTENT_SELECTOR_X - note_w / 2, CONTENT_STATUS_Y, note_size.num, note_size.den);
    for (int t = 0; t < CONTENT_TABS; t++) {
        const int w = o->text_width(o->ctx, TAB_NAME[t], TAB_NUM, TAB_DEN), x = TAB_CENTRE[t] - w / 2, on = t == p->nav->tab;
        o->text(o->ctx, on ? CONTENT_YELLOW : CONTENT_CREAM, TAB_NAME[t], x, CONTENT_TAB_Y, TAB_NUM, TAB_DEN);
        if (on) o->fill(o->ctx, x, CONTENT_TAB_RULE_Y - UNDERLINE_DY, x + w, CONTENT_TAB_RULE_Y - UNDERLINE_DY + 1, YELLOW_RGB);
    }
    rule(o, CONTENT_LIST_X0, CONTENT_TAB_RULE_Y, CONTENT_LIST_X1, CONTENT_TAB_RULE_Y);
    rule(o, CONTENT_SPLIT_X, CONTENT_SPLIT_Y0, CONTENT_SPLIT_X, CONTENT_FOOT_RULE_Y - 1);
}

static void row_line(const ContentOps *o, const ContentPage *p, const ContentRow *r, int line)
{
    const ContentNav *n = p->nav;
    const int y = CONTENT_LIST_Y + line * CONTENT_LINE, lit = !p->quiet && n->at == CONTENT_AT_LIST && n->line == line;
    char name[64], plain[sizeof r->name];
    o->text(o->ctx, !r->switchable ? CONTENT_DIM : lit && n->item == CONTENT_ITEM_BOX ? CONTENT_YELLOW : CONTENT_CREAM, r->on ? "[x]" : "[ ]",
            CONTENT_BOX_X, y, ROW_NUM, ROW_DEN);
    o->text(o->ctx, lit ? CONTENT_YELLOW : r->kind == CONTENT_ROW_REMOVED || p->view->rows->locked ? CONTENT_DIM : CONTENT_CREAM,
            fit(o, clean(r->name, plain, sizeof plain), ROW_NUM, ROW_DEN, CONTENT_NAME_RIGHT - CONTENT_NAME_X, name, sizeof name), CONTENT_NAME_X, y,
            ROW_NUM, ROW_DEN);
    if (r->kind == CONTENT_ROW_MISSING || r->kind == CONTENT_ROW_REMOVED) {   /* in the arrows' place: what the row is */
        const char *word = r->kind == CONTENT_ROW_MISSING ? "MISSING" : "REMOVED";
        o->text(o->ctx, r->kind == CONTENT_ROW_MISSING ? CONTENT_RED : CONTENT_DIM, word, CONTENT_GAP_RIGHT - o->text_width(o->ctx, word, SMALL_NUM, SMALL_DEN),
                y + VERSION_DY, SMALL_NUM, SMALL_DEN);
    }
    if (r->movable && lit) {   /* the arrows show on the row with the focus or the pointer (a move lights the row it is over) */
        o->arrow(o->ctx, CONTENT_ARROW_UP, lit && n->item == CONTENT_ITEM_UP, CONTENT_ARROWS_X, y + ARROW_DY);
        o->arrow(o->ctx, CONTENT_ARROW_DOWN, lit && n->item == CONTENT_ITEM_DOWN, CONTENT_DOWN_X, y + ARROW_DY);
    }
}

/* a locked list's All box: what its rows hold (none can be switched, so content_rows_all would say off) */
static int locked_all(const ContentRows *rows)
{
    int on = 0;
    for (int i = 0; i < rows->count; i++) on += rows->row[i].on;
    return on == 0 ? CONTENT_ALL_OFF : on == rows->count ? CONTENT_ALL_ON : CONTENT_ALL_MIXED;
}

/* the list's pager through the kit (ui_kit.h): the screen's slots in the shape the kit asks for */
static void pager_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    const ContentOps *const o = ctx;
    o->text(o->ctx, style == 1 ? CONTENT_YELLOW : CONTENT_CREAM, text, x, y, 1, 1);
}
static int32_t pager_text_width(void *ctx, const char *text)
{
    const ContentOps *const o = ctx;
    return o->text_width(o->ctx, text, 1, 1);
}
static void pager_scaled(void *ctx, int32_t style, const char *text, int32_t x, int32_t y, int32_t num, int32_t den)
{
    const ContentOps *const o = ctx;
    o->text(o->ctx, style == 1 ? CONTENT_YELLOW : CONTENT_CREAM, text, x, y, num, den);
}
static int32_t pager_scaled_width(void *ctx, const char *text, int32_t num, int32_t den)
{
    const ContentOps *const o = ctx;
    return o->text_width(o->ctx, text, num, den);
}
static int32_t pager_arrow_size(void *ctx, int32_t right, int32_t *w, int32_t *h)
{
    (void)ctx, (void)right;
    *w = UI_ARROW_MASK_W, *h = UI_ARROW_MASK_H;
    return 1;
}
static void pager_arrow(void *ctx, int32_t right, int32_t lit, int32_t x, int32_t y)
{
    const ContentOps *const o = ctx;
    o->arrow(o->ctx, right ? CONTENT_ARROW_RIGHT : CONTENT_ARROW_LEFT, lit, x, y);
}

static void list(const ContentOps *o, const ContentPage *p)
{
    const ContentRows *rows = p->view->rows;
    const ContentNav *n = p->nav;
    const int all = rows->locked ? locked_all(rows) : content_rows_all(rows), lit = !p->quiet && n->at == CONTENT_AT_LIST && n->line == 0;
    const int style = rows->locked ? CONTENT_DIM : lit ? CONTENT_YELLOW : CONTENT_CREAM;
    o->text(o->ctx, style, all == CONTENT_ALL_ON ? "[x]" : all == CONTENT_ALL_MIXED ? "[-]" : "[ ]", CONTENT_BOX_X, CONTENT_LIST_Y, ROW_NUM,
            ROW_DEN);
    o->text(o->ctx, style, "All", CONTENT_NAME_X, CONTENT_LIST_Y, ROW_NUM, ROW_DEN);
    for (int line = 1; line < CONTENT_LINES; line++) {
        const int row = content_nav_row(n, rows, line);
        if (row >= 0) row_line(o, p, &rows->row[row], line);
    }
    const int pages = content_nav_pages(rows);
    if (pages > 1) {   /* the kit's pager: the count at the buttons' size, a triangle only where there is a page to go to */
        const PageNavOps pager = { (void *)o, pager_text, pager_text_width, NULL, pager_arrow_size, pager_arrow, { pager_scaled, pager_scaled, pager_scaled_width }, 0 };
        const UiPagerPlace at = { CONTENT_PAGE_X, CONTENT_PAGE_Y - 4, CONTENT_LIST_X0, CONTENT_LIST_X1 };
        ui_page_count_at(&pager, &at, n->page, pages, 0);
    }
}

/* the card's mod: the installed or removed one of the row's id and the tab's type; NULL for a stock or missing piece */
static const ModInfo *card_mod(const ContentPage *p, const ContentRow *r)
{
    static const int TYPE[CONTENT_TABS] = { MOD_TYPE_CAR, MOD_TYPE_TRACK, MOD_TYPE_TWEAK };
    const ContentMods *m = p->mods;
    if (r->kind == CONTENT_ROW_GAME) return NULL;
    for (int i = 0; i < m->installed_count; i++)
        if (m->installed[i].type == TYPE[p->nav->tab] && strcmp(m->installed[i].id, r->id) == 0) return &m->installed[i];
    for (int i = 0; i < m->removed_count; i++)
        if (strcmp(m->removed[i].id, r->id) == 0) return &m->removed[i];
    return NULL;
}

static const ContentOps *measured;   /* the ops text_wrap measures with */
static int small_width(void *ctx, const char *s, int len)
{
    (void)ctx;
    char run[MOD_INI_DESCRIPTION_MAX + 1];
    snprintf(run, sizeof run, "%.*s", len, s);
    return measured->text_width(measured->ctx, run, SMALL_NUM, SMALL_DEN);
}

static void card_words(const ContentOps *o, const ModInfo *m)
{
    char line[160], version[sizeof m->version], author[sizeof m->author], description[sizeof m->description];
    TextLine lines[CARD_LINES];
    clean(m->version, version, sizeof version), clean(m->author, author, sizeof author), clean(m->description, description, sizeof description);
    if (author[0] != 0) snprintf(line, sizeof line, "v%s - by %s", version, author);
    else snprintf(line, sizeof line, "v%s", version);
    o->text(o->ctx, CONTENT_DIM, line, CONTENT_CARD_X0, CONTENT_CARD_BY_Y, SMALL_NUM, SMALL_DEN);
    measured = o;
    const int n = text_wrap(description, CONTENT_CARD_X1 - CONTENT_CARD_X0, small_width, NULL, lines, CARD_LINES);
    for (int i = 0; i < n; i++) {
        snprintf(line, sizeof line, "%.*s", lines[i].len, description + lines[i].start);
        o->text(o->ctx, CONTENT_CREAM, line, CONTENT_CARD_X0, CONTENT_CARD_TEXT_Y + i * CONTENT_CARD_TEXT_LINE, SMALL_NUM, SMALL_DEN);
    }
}

/* a piece of the game itself (owner's note 5, 2026-11-17): the card says whose it is and what it is, as a mod's says its author and description */
static void game_words(const ContentOps *o, int tab)
{
    static const char *const WHAT[CONTENT_TABS] = { "Original car from the 1996 game", "Original track from the 1996 game", "Original from the 1996 game" };
    o->text(o->ctx, CONTENT_DIM, "ORIGINAL - ELECTRONIC ARTS", CONTENT_CARD_X0, CONTENT_CARD_BY_Y, SMALL_NUM, SMALL_DEN);
    o->text(o->ctx, CONTENT_CREAM, WHAT[tab], CONTENT_CARD_X0, CONTENT_CARD_TEXT_Y, SMALL_NUM, SMALL_DEN);
}

static void card(const ContentOps *o, const ContentPage *p)
{
    /* with no row selected (the pointer on the tabs, the All row) the page's first row's card stands, its buttons grey (card_on is off
     * with no row): the panel is never empty */
    int row = content_nav_selected_row(p->nav, p->view->rows);
    if (row < 0) row = content_nav_row(p->nav, p->view->rows, 1);
    if (row < 0) return;
    const ContentRow *r = &p->view->rows->row[row];
    o->fill(o->ctx, CONTENT_CARD_X0, CONTENT_PREVIEW_Y0, CONTENT_CARD_X1, CONTENT_PREVIEW_Y1, CONTENT_RULE_RGB);
    o->fill(o->ctx, CONTENT_CARD_X0 + 1, CONTENT_PREVIEW_Y0 + 1, CONTENT_CARD_X1 - 1, CONTENT_PREVIEW_Y1 - 1, CONTENT_PANEL_RGB);
    o->preview(o->ctx, p->nav->tab, r->id, CONTENT_CARD_X0 + 1, CONTENT_PREVIEW_Y0 + 1, CONTENT_CARD_X1 - 1, CONTENT_PREVIEW_Y1 - 1);
    char text[64], plain[sizeof r->name];
    o->text(o->ctx, CONTENT_YELLOW, fit(o, clean(r->name, plain, sizeof plain), 1, 1, CONTENT_CARD_X1 - CONTENT_CARD_X0, text, sizeof text), CONTENT_CARD_X0,
            CONTENT_CARD_NAME_Y, 1, 1);
    const ModInfo *m = card_mod(p, r);
    if (m != NULL) card_words(o, m);
    else if (r->kind == CONTENT_ROW_GAME) game_words(o, p->nav->tab);
    /* a piece of the game is in the standard edition (Restomod) whatever the player's editions say: at least 1 (owner's note 10) */
    const int used = o->used_in(o->ctx, r->id) + (r->kind == CONTENT_ROW_GAME ? 1 : 0);
    snprintf(text, sizeof text, "Used in: %d edition%s", used, used == 1 ? "" : "s");
    o->text(o->ctx, CONTENT_CREAM, text, CONTENT_CARD_X0, CONTENT_CARD_USED_Y, SMALL_NUM, SMALL_DEN);
    for (int b = 0; b < CONTENT_CARD_BUTTONS; b++) {
        /* a button that never applies is not drawn: none of them on a piece of the game, only Previous version on a removed one */
        if (r->kind == CONTENT_ROW_GAME || (r->kind == CONTENT_ROW_REMOVED && b != CONTENT_CARD_PREVIOUS)) continue;
        o->text(o->ctx, button_style(p->view->card_on[b], !p->quiet && p->nav->at == CONTENT_AT_CARD && p->nav->item == b), CARD_WORDS[b], CARD_X[b],
                CARD_Y[b], BUTTON_NUM, BUTTON_DEN);
    }
}

static void foot(const ContentOps *o, const ContentPage *p)
{
    rule(o, CONTENT_LIST_X0, CONTENT_FOOT_RULE_Y, CONTENT_CARD_X1, CONTENT_FOOT_RULE_Y);
    for (int b = 0; b < CONTENT_FOOT_BUTTONS; b++) {
        const int style = button_style(p->view->foot_on[b], !p->quiet && p->nav->at == CONTENT_AT_FOOT && p->nav->item == b);
        if (b == CONTENT_FOOT_DONE)   /* the CD's DONE: at the left, the capitals 11 rows */
            o->text(o->ctx, style, FOOT_WORDS[b], CONTENT_DONE_X, CONTENT_FOOT_Y - DONE_DY, DONE_NUM, DONE_DEN);
        else
            o->text(o->ctx, style, FOOT_WORDS[b], FOOT_X[b] + CONTENT_FOOT_SHIFT, CONTENT_FOOT_Y, BUTTON_NUM, BUTTON_DEN);
    }
}

void content_draw(const ContentOps *o, const ContentPage *p)
{
    const ContentNav *n = p->nav;
    o->background(o->ctx);
    top(o, p);
    list(o, p);
    card(o, p);
    foot(o, p);
    o->show(o->ctx);
    port_log("mark:content=%d %d %d %d %d\n", n->tab, n->at, n->line, n->item, n->page);   /* a WAIT's text is < 32 */
}
