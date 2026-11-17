/* See content_draw.h. */
#include "game/frontend/content_draw.h"

#include "game/frontend/text_wrap.h"
#include "game/sys/port_log.h"

#include <stdio.h>
#include <string.h>

/* the sizes, num/den of the font's: GRAVER18 is wide, so the list's words and the tabs (3/4, as Control Central's card names) are
 * smaller than the mock-up's Mac font drew them, the versions and the card's words smaller still (3/5) */
enum { SMALL_NUM = 3, SMALL_DEN = 5, BUTTON_NUM = 4, BUTTON_DEN = 5, ROW_NUM = 3, ROW_DEN = 4, TAB_NUM = 3, TAB_DEN = 4, GAP = 8, ARROW_DY = 3,
       VERSION_DY = 2, CARD_LINES = 2 };

static const char *const TAB_NAME[CONTENT_TABS] = { "Cars", "Tracks", "Tweaks" };
static const int TAB_CENTRE[CONTENT_TABS] = { 90, 197, 304 };   /* centred over the list, fixed places */
static const char *const CARD_WORDS[CONTENT_CARD_BUTTONS] = { "Update...", "Uninstall", "Previous version" };
static const int CARD_X[CONTENT_CARD_BUTTONS] = { CONTENT_CARD_X0, CONTENT_UNINSTALL_X, CONTENT_CARD_X0 };
static const int CARD_Y[CONTENT_CARD_BUTTONS] = { CONTENT_CARD_BUTTONS_Y, CONTENT_CARD_BUTTONS_Y, CONTENT_PREVIOUS_Y };
static const char *const FOOT_WORDS[CONTENT_FOOT_BUTTONS] = { "New", "Duplicate", "Rename", "Delete", "Install...", "Done" };
static const int FOOT_X[CONTENT_FOOT_BUTTONS - 1] = { 14, 66, 170, 252, 420 };
static const char READ_ONLY[] = "Standard edition - read only. Duplicate to edit.";
static const char YOURS[] = "Your edition - in use";
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
    o->arrow(o->ctx, CONTENT_ARROW_LEFT, sel, CONTENT_SELECTOR_ARROW_L, CONTENT_SELECTOR_Y + ARROW_DY);
    o->arrow(o->ctx, CONTENT_ARROW_RIGHT, sel, CONTENT_SELECTOR_ARROW_R, CONTENT_SELECTOR_Y + ARROW_DY);
    fit(o, clean(p->edition, plain, sizeof plain), 1, 1, CONTENT_SELECTOR_NAME_W, name, sizeof name);
    o->text(o->ctx, sel ? CONTENT_YELLOW : CONTENT_CREAM, name, CONTENT_SELECTOR_X - o->text_width(o->ctx, name, 1, 1) / 2, CONTENT_SELECTOR_Y, 1, 1);
    const char *status = p->view->rows->locked ? READ_ONLY : YOURS;
    o->text(o->ctx, p->flash ? CONTENT_YELLOW : CONTENT_DIM, status, CONTENT_SELECTOR_X - o->text_width(o->ctx, status, SMALL_NUM, SMALL_DEN) / 2,
            CONTENT_STATUS_Y, SMALL_NUM, SMALL_DEN);
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
    if (pages > 1) {   /* the two arrows always, dim when there is no page that way */
        char text[16];
        snprintf(text, sizeof text, "%d/%d", n->page + 1, pages);
        o->text(o->ctx, CONTENT_CREAM, text, CONTENT_PAGE_X - o->text_width(o->ctx, text, SMALL_NUM, SMALL_DEN) / 2, CONTENT_PAGE_Y, SMALL_NUM, SMALL_DEN);
        o->arrow(o->ctx, n->page > 0 ? CONTENT_ARROW_LEFT : CONTENT_ARROW_LEFT_OFF, 0, CONTENT_PAGE_PREV_X, CONTENT_PAGE_Y + ARROW_DY);
        o->arrow(o->ctx, n->page + 1 < pages ? CONTENT_ARROW_RIGHT : CONTENT_ARROW_RIGHT_OFF, 0, CONTENT_PAGE_NEXT_X, CONTENT_PAGE_Y + ARROW_DY);
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
    const int row = content_nav_selected_row(p->nav, p->view->rows);
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
        if (b == CONTENT_FOOT_DONE)
            o->text(o->ctx, style, FOOT_WORDS[b], CONTENT_DONE_RIGHT - o->text_width(o->ctx, FOOT_WORDS[b], 1, 1), CONTENT_FOOT_Y - 2, 1, 1);
        else
            o->text(o->ctx, style, FOOT_WORDS[b], FOOT_X[b], CONTENT_FOOT_Y, BUTTON_NUM, BUTTON_DEN);
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
