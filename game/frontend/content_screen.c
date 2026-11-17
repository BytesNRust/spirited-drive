/* See content_screen.h. */
#include "game/frontend/content_screen.h"

#include "game/frontend/content_draw.h"
#include "game/sys/port_log.h"

#include <string.h>

enum { LEFT_BUTTON = 1 };

typedef struct {
    int32_t edition, count;
    int own;                 /* the edition is one of the player's (read into e) */
    Edition e;
    ContentMods mods;
    ContentRows rows;
    ContentNav nav;
    ContentView view;
    char name[EDITION_NAME_MAX + 16];
    int done;
} Screen;

static const Edition *edition_of(const Screen *s) { return s->own ? &s->e : NULL; }

/* the rows of the tab, the buttons the selection can take, the selection kept on what is there */
static void rebuild(Screen *s)
{
    content_rows_build(&s->rows, edition_of(s), s->nav.tab, &s->mods);
    s->view.rows = &s->rows;
    content_nav_clamp(&s->nav, &s->view);
    const int row = content_nav_selected_row(&s->nav, &s->rows);
    const ContentRow *r = row >= 0 ? &s->rows.row[row] : NULL;
    s->view.card_on[CONTENT_CARD_UPDATE] = s->view.card_on[CONTENT_CARD_UNINSTALL] = r != NULL && r->kind == CONTENT_ROW_MOD;
    s->view.card_on[CONTENT_CARD_PREVIOUS] = r != NULL && r->backup;
    for (int b = 0; b < CONTENT_FOOT_BUTTONS; b++) s->view.foot_on[b] = 1;
    s->view.foot_on[CONTENT_FOOT_RENAME] = s->view.foot_on[CONTENT_FOOT_DELETE] = s->own;
}

/* the editions, the one shown, and the mods read again */
static void load(Screen *s, const ContentOps *o)
{
    s->count = o->editions(o->ctx);
    if (s->count < 1) s->count = 1;
    if (s->edition < 0 || s->edition >= s->count) s->edition = 0;
    memset(&s->e, 0, sizeof s->e);
    s->own = s->edition > 0 && o->edition_load(o->ctx, s->edition, &s->e);
    o->edition_name(o->ctx, s->edition, s->name, sizeof s->name);
    o->mods(o->ctx, &s->mods);
    rebuild(s);
}

/* an edit of the edition: saved at once, then shown; 1 when it was */
static int edited(Screen *s, const ContentOps *o, int made, const Edition *out)
{
    if (!made) return 0;
    if (!o->edition_save(o->ctx, s->edition, out)) {
        port_log("content: the edition %s was not saved\n", s->name);
        return 0;
    }
    s->e = *out;
    rebuild(s);
    return 1;
}

static void leave(Screen *s, const ContentOps *o)
{
    const int lacks = content_rules_ok(edition_of(s), &s->mods);
    if (lacks != CONTENT_RULES_OK) o->refused(o->ctx, lacks);
    else s->done = 1;
}

static void perform(Screen *s, const ContentOps *o, ContentAction a)
{
    Edition out;
    switch (a.kind) {
    case CONTENT_ACT_EDITION:
        s->edition = (s->edition + a.arg + s->count) % s->count;
        load(s, o);
        break;
    case CONTENT_ACT_SWITCH: edited(s, o, content_rows_switch(edition_of(s), &out, s->nav.tab, &s->rows, a.row), &out); break;
    case CONTENT_ACT_ALL: edited(s, o, content_rows_switch_all(edition_of(s), &out, s->nav.tab, &s->rows), &out); break;
    case CONTENT_ACT_MOVE:
        if (edited(s, o, content_rows_move(edition_of(s), &out, s->nav.tab, &s->rows, a.row, a.arg), &out)) {
            if (a.arg < 0 ? s->nav.line > 1 : s->nav.line < CONTENT_LINES - 1) s->nav.line += a.arg;   /* the selection goes with the row */
            rebuild(s);
        }
        break;
    case CONTENT_ACT_CARD:
        if (o->card(o->ctx, a.arg, s->nav.tab, s->rows.row[a.row].id)) load(s, o);
        break;
    case CONTENT_ACT_FOOT:
        if (a.arg == CONTENT_FOOT_DONE) leave(s, o);
        else if (o->foot(o->ctx, a.arg, &s->edition)) load(s, o);
        break;
    default: rebuild(s); break;   /* MOVED, TAB: the card and the rows follow the selection */
    }
}

void content_screen_run(const ContentOps *o, int32_t *edition)
{
    port_log("mark:screen=content\n");   /* port: the walks wait for it */
    static Screen s;   /* the rows are 20 KB: off the stack (the web build's is 64 KB) */
    memset(&s, 0, sizeof s);
    s.edition = *edition;
    content_nav_start(&s.nav);
    load(&s, o);
    ContentPage page = { s.name, &s.mods, &s.view, &s.nav };
    int32_t px = -1, py = -1, x, y;
    int32_t buttons = o->pointer(o->ctx, &px, &py);   /* a button or a key still down from what opened the screen is not taken */
    int armed = 0, redraw = 1;
    while (!s.done) {
        if (redraw) page.edition = s.name, content_draw(o, &page), redraw = 0;
        const int32_t key = o->key(o->ctx), now = o->pointer(o->ctx, &x, &y);
        ContentAction a = { CONTENT_ACT_NONE, 0, 0 };
        if (key == 0) armed = 1;
        else if (armed) a = content_nav_key(&s.nav, &s.view, key), armed = 0;
        if (a.kind == CONTENT_ACT_NONE && (now & LEFT_BUTTON) && !(buttons & LEFT_BUTTON)) a = content_nav_click(&s.nav, &s.view, x, y);
        else if (a.kind == CONTENT_ACT_NONE && (x != px || y != py)) a = content_nav_point(&s.nav, &s.view, x, y);
        buttons = now, px = x, py = y;
        if (a.kind != CONTENT_ACT_NONE) perform(&s, o, a), redraw = 1;
        if (o->dropped != NULL && o->dropped(o->ctx, &s.edition)) load(&s, o), redraw = 1;   /* port (T7): a file dropped, installed */
        o->wait(o->ctx);
    }
    *edition = s.edition;
}
