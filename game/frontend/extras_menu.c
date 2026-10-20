/* port: see extras_menu.h. Two pages of switches, drawn over the live screen's picture. The plan's steps:
 * Mirror and Rally are mutually exclusive (turning one on turns the other off), each switch flips on Space
 * (or Right/Left arrows on a switch row), PageDown (or Right on the last row) turns to page 2, Done saves,
 * Cancel and Escape leave the input PortExtras and unlock level untouched. */
#include "game/frontend/extras_menu.h"

#include <stdio.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       PAGE_DOWN = 0x5100 };
enum { MOVED = 0x47, CHOSEN = 0x48, REST = 0x19 };
enum { NAV_CENTRE = 320, NAV_Y = 444 };
enum { PANEL_X0 = 4, PANEL_Y0 = 96, PANEL_X1 = 635, PANEL_Y1 = 404 };
enum { LABEL_X = 8, VALUE_RIGHT = 493 };
enum { PAGES = 2 };

/* the bands for follow(): the page-1 selectable rows are in BAND_*_P1, page-2 in BAND_*_P2 */
typedef struct Page {
    int rows;
    int16_t selected;
    int redraw, armed, released;
    int32_t rest;
    int result;   /* 1 Done, 0 Cancel/Escape, -1 still running */
} Page;

/* render rows on a 48-pixel-tall band (the label is a single line) */
static const int16_t BAND_TOPS_P1[ROWS_PAGE_1] = { 106, 159, 210, 262, 315, 0, 0, 0, 445, 445 };
static const int16_t BAND_BOTS_P1[ROWS_PAGE_1] = { 132, 185, 236, 288, 341, 0, 0, 0, 470, 470 };
static const int16_t BAND_TOPS_P2[ROWS_PAGE_2] = { 106, 159, 210, 262, 315, 445, 445 };
static const int16_t BAND_BOTS_P2[ROWS_PAGE_2] = { 132, 185, 236, 288, 341, 470, 470 };

/* helper: pull the value of a page-1 trick row into `int *val` (excludes Done / Cancel) */
static void p1_get(int row, const PortExtras *e, int *val)
{
    switch (row) {
    case ROW_MIRROR: *val = e->mirror; break;
    case ROW_RALLY: *val = e->rally; break;
    case ROW_WARRIOR: *val = e->warrior; break;
    case ROW_VEGAS: *val = e->vegas; break;
    case ROW_EXPLODE: *val = e->explode; break;
    case ROW_NITRO: *val = e->nitro; break;
    case ROW_TURBO: *val = e->turbo; break;
    default: *val = 0; break;
    }
}

static void mirror_rally_exclusive(PortExtras *e)
{
    if (e->mirror && e->rally) e->rally = 0;
}

static void label_row1(int row, char out[24])
{
    static const char *const L[ROWS_PAGE_1] = {
        "Mirror", "Rally", "Warrior", "Lost Vegas", "Explode", "Nitro", "Turbo", "Unlock Level"
    };
    snprintf(out, 24, "%s", L[row]);
}

static void value_row1(int row, const PortExtras *e, int16_t unlock, char out[16])
{
    int v = 0;
    p1_get(row, e, &v);
    switch (row) {
    case ROW_UNLOCK: snprintf(out, 16, "%d", unlock); break;
    default: snprintf(out, 16, "%s", v ? "On" : "Off"); break;
    }
}

static void nav_draw(int page, const ExtrasMenuOps *o)
{
    const char *text = page == 0 ? "1/2 >" : "< 2/2";
    o->plus_text(o->ctx, 0, text, NAV_CENTRE - o->plus_text_width(o->ctx, text) / 2, NAV_Y);
}

static void draw_page(Page *p, PortExtras *e, int16_t unlock, int page, const ExtrasMenuOps *o)
{
    o->select(o->ctx, 0);
    o->blit_own(o->ctx, o->shape(o->ctx, 0, "bgnd"));
    o->plus_panel(o->ctx, NULL, PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, 1);
    if (page == 0) {
        char l[24], v[16];
        for (int row = 0; row < ROW_DONE; row++) {
            label_row1(row, l);
            value_row1(row, e, unlock, v);
            o->plus_text(o->ctx, p->selected == row ? 1 : 0, l, LABEL_X, BAND_TOPS_P1[row]);
            o->plus_text(o->ctx, p->selected == row ? 1 : 0, v, VALUE_RIGHT - o->plus_text_width(o->ctx, v),
                         BAND_TOPS_P1[row]);
        }
        if (p->selected == ROW_DONE) o->plus_text(o->ctx, 1, "Done", LABEL_X, BAND_TOPS_P1[ROW_DONE]);
        if (p->selected == ROW_CANCEL_P1) o->plus_text(o->ctx, 1, "Cancel", LABEL_X + 400, BAND_TOPS_P1[ROW_DONE]);
    } else {
        /* page 2 */
        char *labels[ROWS_PAGE_2] = { "Replay Cameras", "Warrior as Rival", "No Movies", "Start Screen",
                                       "No Assists", "Done", "Cancel" };
        char *values[ROWS_PAGE_2] = { NULL };
        static char start_text[16];
        if (e->start == 0) snprintf(start_text, sizeof start_text, "Game Mode");
        else if (e->start == 1) snprintf(start_text, sizeof start_text, "Network");
        else snprintf(start_text, sizeof start_text, "Control");
        values[ROW_REPLAY] = (char *)(e->replay_cameras ? "On" : "Off");
        values[ROW_RIVAL] = (char *)(e->warrior_rival ? "On" : "Off");
        values[ROW_MOVIES] = (char *)(e->no_movies ? "On" : "Off");
        values[ROW_START] = start_text;
        values[ROW_NO_ASSISTS] = (char *)(e->no_assists ? "On" : "Off");
        for (int row = 0; row < ROWS_PAGE_2; row++) {
            o->plus_text(o->ctx, p->selected == row ? 1 : 0, labels[row], LABEL_X, BAND_TOPS_P2[row]);
            if (values[row])
                o->plus_text(o->ctx, p->selected == row ? 1 : 0, values[row],
                             VALUE_RIGHT - o->plus_text_width(o->ctx, values[row]), BAND_TOPS_P2[row]);
        }
    }
    nav_draw(page, o);
    o->select_default(o->ctx);
    o->flip(o->ctx);
    p->redraw = 0;
}

static void follow(Page *p, int page, const ExtrasMenuOps *o)
{
    if (o->pointer_in_rect(o->ctx, EXTRAS_NAV_X0, EXTRAS_NAV_X1, EXTRAS_NAV_Y0, EXTRAS_NAV_Y1)) return;
    if (page == 0) {
        for (int row = 0; row < ROWS_PAGE_1; row++)
            if (o->pointer_in_rect(o->ctx, 0, 0x280, BAND_TOPS_P1[row], BAND_BOTS_P1[row])) {
                if (p->selected != row) {
                    p->selected = (int16_t)row;
                    p->redraw = 1;
                    o->sound(o->ctx, MOVED);
                }
                return;
            }
    } else {
        for (int row = 0; row < ROWS_PAGE_2; row++)
            if (o->pointer_in_rect(o->ctx, 0, 0x280, BAND_TOPS_P2[row], BAND_BOTS_P2[row])) {
                if (p->selected != row) {
                    p->selected = (int16_t)row;
                    p->redraw = 1;
                    o->sound(o->ctx, MOVED);
                }
                return;
            }
    }
}

static void select_row(Page *p, int row, const ExtrasMenuOps *o)
{
    if (p->selected == row) return;
    p->selected = (int16_t)row;
    p->redraw = 1;
    o->sound(o->ctx, MOVED);
}

static void flip_row1(int row, PortExtras *e)
{
    int v = 0;
    p1_get(row, e, &v);
    v = !v;
    switch (row) {
    case ROW_MIRROR: e->mirror = v; mirror_rally_exclusive(e); break;
    case ROW_RALLY: e->rally = v; mirror_rally_exclusive(e); break;
    case ROW_WARRIOR: e->warrior = v; break;
    case ROW_VEGAS: e->vegas = v; break;
    case ROW_EXPLODE: e->explode = v; break;
    case ROW_NITRO: e->nitro = v; break;
    case ROW_TURBO: e->turbo = v; break;
    default: break;
    }
}

static int choose(Page *p, PortExtras *e, int16_t *unlock, int *page_inout, const ExtrasMenuOps *o)
{
    o->pointer_click(o->ctx);
    if (!p->armed) return -1;
    o->sound(o->ctx, CHOSEN);
    int row = p->selected;
    if (*page_inout == 0) {
        if (row < ROW_DONE) flip_row1(row, e);
        else if (row == ROW_DONE) { o->extras_save(o->ctx, e, *unlock); return 1; }
        else if (row == ROW_CANCEL_P1) return 0;
    } else {
        switch (row) {
        case ROW_REPLAY: e->replay_cameras = !e->replay_cameras; break;
        case ROW_RIVAL: e->warrior_rival = !e->warrior_rival; break;
        case ROW_MOVIES: e->no_movies = !e->no_movies; break;
        case ROW_START: e->start = (e->start + 1) % 3; break;
        case ROW_NO_ASSISTS: e->no_assists = !e->no_assists; break;
        case ROW_DONE_P2: o->extras_save(o->ctx, e, *unlock); return 1;
        case ROW_CANCEL_P2: return 0;
        }
    }
    p->redraw = 1;
    return -1;
}

static int step(Page *p, uint16_t k, PortExtras *e, int16_t *unlock, int *page_inout, const ExtrasMenuOps *o)
{
    if (k == ESCAPE) {
        o->sound(o->ctx, CHOSEN);
        return 0;
    }
    if (k == PAGE_DOWN) {
        *page_inout = 1 - *page_inout;
        p->selected = 0;
        p->redraw = 1;
        o->sound(o->ctx, CHOSEN);
        return -1;
    }
    int rows = (*page_inout == 0) ? ROWS_PAGE_1 : ROWS_PAGE_2;
    int from = (rows > 0 && p->selected == rows - 1) ? 0 : p->selected;
    int to;
    if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        to = (k == DOWN) ? (from + 1) % rows : (from + rows - 1) % rows;
        select_row(p, to, o);
    } else if (k == LEFT || k == RIGHT) {
        int last = rows - 1;
        if (p->selected == last - 1 || p->selected == last) {
            o->pointer_off(o->ctx);
            select_row(p, p->selected == last - 1 ? last : last - 1, o);
        } else if (*page_inout == 0 && p->selected == ROW_UNLOCK) {
            *unlock = (int16_t)((*unlock + (k == RIGHT ? 1 : 2) + 3) % 3);
            o->sound(o->ctx, CHOSEN);
            p->redraw = 1;
        } else if (*page_inout == 1) {
            switch (p->selected) {
            case ROW_REPLAY: e->replay_cameras = !e->replay_cameras; break;
            case ROW_RIVAL: e->warrior_rival = !e->warrior_rival; break;
            case ROW_MOVIES: e->no_movies = !e->no_movies; break;
            case ROW_START: e->start = (int16_t)((e->start + (k == RIGHT ? 1 : 2) + 3) % 3); break;
            case ROW_NO_ASSISTS: e->no_assists = !e->no_assists; break;
            default: break;
            }
            o->sound(o->ctx, CHOSEN);
            p->redraw = 1;
        } else if (*page_inout == 0) {
            int v = 0;
            p1_get(p->selected, e, &v);
            v = !v;
            /* row 0 only: simulate a flip */
            if (p->selected == ROW_MIRROR) { e->mirror = v; mirror_rally_exclusive(e); }
            else if (p->selected == ROW_RALLY) { e->rally = v; mirror_rally_exclusive(e); }
            else if (p->selected == ROW_WARRIOR) e->warrior = v;
            else if (p->selected == ROW_VEGAS) e->vegas = v;
            else if (p->selected == ROW_EXPLODE) e->explode = v;
            else if (p->selected == ROW_NITRO) e->nitro = v;
            else if (p->selected == ROW_TURBO) e->turbo = v;
            o->sound(o->ctx, CHOSEN);
            p->redraw = 1;
        }
    }
    return -1;
}

int extras_menu_run(PortExtras *e, int16_t *unlock, const ExtrasMenuOps *o)
{
    Page p = { ROWS_PAGE_1, (*e).mirror ? ROW_MIRROR : ROW_MIRROR, 1, 1, 1, 0, -1 };
    int page = 0;
    (void)ROWS_PAGE_1;
    (void)ROWS_PAGE_2;
    /* port: a snapshot taken at the start; on Cancel and Escape we put them back, per the plan */
    const PortExtras input = *e;
    const int16_t input_unlock = *unlock;
    draw_page(&p, e, *unlock, page, o);
    while (p.result < 0) {
        follow(&p, page, o);
        if (p.redraw) draw_page(&p, e, *unlock, page, o);
        if (!p.armed) {
            p.rest += o->ticks(o->ctx);
            if ((int16_t)p.rest > REST) p.armed = 1;
        }
        const uint16_t k = (uint16_t)o->key(o->ctx);
        if (k == SPACE || k == ENTER) {
            const int r = choose(&p, e, unlock, &page, o);
            if (r >= 0) p.result = r;
        } else if (k == 0) {
            p.armed = 1;
        } else if (p.armed) {
            const int r = step(&p, k, e, unlock, &page, o);
            if (r >= 0) p.result = r;
        }
        o->music(o->ctx);
        p.armed = 0;
        p.rest = 0;
    }
    if (p.result != 1) {
        /* port: Cancel and Escape leave the input unchanged */
        *e = input;
        *unlock = input_unlock;
    }
    return p.result == 1 ? 1 : 0;
}
