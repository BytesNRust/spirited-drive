/* port: see extras_menu.h. Two pages of switches drawn with the game's text over the live screen's picture.
 * Up / Down step through a page's rows (Done and Cancel last); Enter, Space, Left or Right change a switch (Unlock
 * and Start step through their values); Left / Right swap Done and Cancel, Right on Cancel turns to the next page and
 * Left on Done back to the one before; a click on the page indicator or PageDown turns to the next, round to the first. Mirror and Rally exclude each
 * other, unless ExtrasMenuOps.port_combine_variants lets them both stay on. Done saves; Cancel and Escape put the
 * input back. */
#include "game/frontend/extras_menu.h"

#include <stdio.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       PAGE_DOWN = 0x5100 };
enum { MOVED = 0x47, CHOSEN = 0x48, REST = 0x19, WIDE = 0x280, LEFT_BUTTON = 1 };
enum { PANEL_X0 = 4, PANEL_Y0 = 96, PANEL_X1 = 635, PANEL_Y1 = 404 };
enum { LABEL_X = 16, VALUE_RIGHT = 600, ROW_TOP = 106, ROW_STEP = 36, ROW_TALL = 30 };
enum { BUTTON_Y = 445, DONE_X = 40, CANCEL_RIGHT = 600, DONE_X1 = 0x141, CANCEL_X0 = 0x143, BUTTONS_Y0 = 432,
       BUTTONS_Y1 = 476 };

/* the switches of each page, Done and Cancel after them */
enum { PAGES = 3 };
static const int SWITCHES[PAGES] = { ROW_DONE, ROW_DONE_P2, ROW_DONE_P3 };

typedef struct Page {
    int page;
    int16_t selected;
    int redraw, armed;
    int released;   /* the buttons seen up since the last click */
    int32_t rest;
    int result;   /* 1 Done, 0 Cancel / Escape, -1 still running */
} Page;

static int rows_of(int page) { return SWITCHES[page] + 2; }
static int done_of(int page) { return SWITCHES[page]; }
static int cancel_of(int page) { return SWITCHES[page] + 1; }

static const char *label(int page, int row)
{
    static const char *const P1[ROW_DONE] = { "Mirror", "Rally", "Warrior", "Lost Vegas", "Explode", "Nitro", "Turbo",
                                              "Unlock Level" };
    static const char *const P2[ROW_DONE_P2] = { "Replay Cameras", "Warrior as Rival", "No Movies", "Start Screen",
                                                 "No Assists", "Modern 3D", "Texture Filter", "Fog" };
    static const char *const P3[ROW_DONE_P3] = { "Bumper Mirror", "Bumper Radar" };
    return page == 0 ? P1[row] : page == 1 ? P2[row] : P3[row];
}

/* a switch's field, NULL for the rows with more than two values (Unlock, Start) */
static int *flag_of(PortExtras *e, int page, int row)
{
    if (page == 0) {
        int *const p1[ROW_DONE] = { &e->mirror, &e->rally, &e->warrior, &e->vegas, &e->explode, &e->nitro, &e->turbo,
                                    NULL };
        return p1[row];
    }
    if (page == 1) {
        int *const p2[ROW_DONE_P2] = { &e->replay_cameras, &e->warrior_rival, &e->no_movies, NULL, &e->no_assists,
                                       &e->modern_3d, &e->texture_filter, &e->fog };
        return p2[row];
    }
    int *const p3[ROW_DONE_P3] = { &e->bumper_mirror, &e->bumper_radar };
    return p3[row];
}

static void value(PortExtras *e, int16_t unlock, int page, int row, char out[16])
{
    static const char *const START[3] = { "Game Mode", "Network", "Control Central" };
    const int *const flag = flag_of(e, page, row);
    if (flag != NULL) snprintf(out, 16, "%s", *flag ? "On" : "Off");
    else if (page == 0) snprintf(out, 16, "%d", unlock);
    else snprintf(out, 16, "%s", START[(unsigned)e->start % 3]);
}

static int16_t row_top(int row) { return (int16_t)(ROW_TOP + row * ROW_STEP); }

static void draw_page(Page *p, PortExtras *e, int16_t unlock, int32_t bank, const ExtrasMenuOps *o)
{
    o->select(o->ctx, 0);   /* the caller's picture behind */
    o->plus_panel(o->ctx, NULL, PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, 1);
    for (int row = 0; row < SWITCHES[p->page]; row++) {
        char v[16];
        const int lit = p->selected == row;
        value(e, unlock, p->page, row, v);
        o->plus_text(o->ctx, lit, label(p->page, row), LABEL_X, row_top(row));
        o->plus_text(o->ctx, lit, v, VALUE_RIGHT - o->plus_text_width(o->ctx, v), row_top(row));
    }
    /* the bottom row's Done and Cancel: the exe's Options look — both are always drawn (lit when selected, dark
     * otherwise), the same way Options' rows paint their labels. The art replaces the flat panel + "Done"/"Cancel"
     * text. */
    o->blit_own(o->ctx, o->shape(o->ctx, bank, p->selected == done_of(p->page) ? "donl" : "dond"));
    o->blit_own(o->ctx, o->shape(o->ctx, bank, p->selected == cancel_of(p->page) ? "canl" : "cand"));
    static const char *const NAV[PAGES] = { "1/3 >", "< 2/3 >", "< 3/3" };
    const char *const nav = NAV[p->page];
    o->plus_text(o->ctx, 0, nav, (EXTRAS_NAV_X0 + EXTRAS_NAV_X1) / 2 - o->plus_text_width(o->ctx, nav) / 2, BUTTON_Y);
    o->select_default(o->ctx);
    o->flip(o->ctx);
    p->redraw = 0;
}

static void select_row(Page *p, int row, const ExtrasMenuOps *o)
{
    if (p->selected == row) return;
    p->selected = (int16_t)row;
    p->redraw = 1;
    o->sound(o->ctx, MOVED);
}

/* the keys take the pointer along to the row they select, so the pointer's band keeps it (as Options' move_to) */
static void pointer_to(const Page *p, const ExtrasMenuOps *o)
{
    enum { HALF_ROW = ROW_TALL / 2, BUTTON_MID = (BUTTONS_Y0 + BUTTONS_Y1) / 2 };
    if (p->selected == done_of(p->page)) o->pointer_move_to(o->ctx, DONE_X + 20, BUTTON_MID);
    else if (p->selected == cancel_of(p->page)) o->pointer_move_to(o->ctx, CANCEL_RIGHT - 30, BUTTON_MID);
    else o->pointer_move_to(o->ctx, LABEL_X + 40, row_top(p->selected) + HALF_ROW);
}

static void turn(Page *p, int page, int row, const ExtrasMenuOps *o)
{
    p->page = page;
    p->selected = (int16_t)row;
    p->redraw = 1;
    o->sound(o->ctx, CHOSEN);
    pointer_to(p, o);
}

/* the first band the pointer is in becomes the selection; the page indicator selects nothing */
static void follow(Page *p, const ExtrasMenuOps *o)
{
    if (o->pointer_in_rect(o->ctx, EXTRAS_NAV_X0, EXTRAS_NAV_X1, EXTRAS_NAV_Y0, EXTRAS_NAV_Y1)) return;
    for (int row = 0; row < SWITCHES[p->page]; row++)
        if (o->pointer_in_rect(o->ctx, 0, WIDE, row_top(row), row_top(row) + ROW_TALL)) {
            select_row(p, row, o);
            return;
        }
    if (o->pointer_in_rect(o->ctx, 0, DONE_X1, BUTTONS_Y0, BUTTONS_Y1)) select_row(p, done_of(p->page), o);
    else if (o->pointer_in_rect(o->ctx, CANCEL_X0, WIDE, BUTTONS_Y0, BUTTONS_Y1)) select_row(p, cancel_of(p->page), o);
}

/* a switch row changed by `step` (+1 or -1): flags flip, Unlock and Start step through their three values */
static void change(Page *p, PortExtras *e, int16_t *unlock, int step, const ExtrasMenuOps *o)
{
    int *const flag = flag_of(e, p->page, p->selected);
    if (flag != NULL) {
        *flag = !*flag;
        /* port: with the two allowed together, turning one on leaves the other as it is; the exe's screen clears
         * it, so the pair stays behind the flag. */
        if (o->port_combine_variants == 0) {
            if (*flag && flag == &e->mirror) e->rally = 0;
            if (*flag && flag == &e->rally) e->mirror = 0;
        }
    } else if (p->page == 0) {
        *unlock = (int16_t)((*unlock + step + 3) % 3);
    } else {
        e->start = (StartScreen)((e->start + step + 3) % 3);
    }
    p->redraw = 1;
    o->sound(o->ctx, CHOSEN);
}

static void choose(Page *p, PortExtras *e, int16_t *unlock, const ExtrasMenuOps *o)
{
    o->pointer_click(o->ctx);
    if (o->pointer_in_rect(o->ctx, EXTRAS_NAV_X0, EXTRAS_NAV_X1, EXTRAS_NAV_Y0, EXTRAS_NAV_Y1)) {
        turn(p, (p->page + 1) % PAGES, 0, o);
    } else if (p->selected == done_of(p->page)) {
        o->sound(o->ctx, CHOSEN);
        o->extras_save(o->ctx, e, *unlock);
        p->result = 1;
    } else if (p->selected == cancel_of(p->page)) {
        o->sound(o->ctx, CHOSEN);
        p->result = 0;
    } else {
        change(p, e, unlock, 1, o);
    }
}

static void step(Page *p, uint16_t k, PortExtras *e, int16_t *unlock, const ExtrasMenuOps *o)
{
    const int rows = rows_of(p->page), done = done_of(p->page), cancel = cancel_of(p->page);
    if (k == ESCAPE) {
        o->sound(o->ctx, CHOSEN);
        p->result = 0;
    } else if (k == PAGE_DOWN) {
        turn(p, (p->page + 1) % PAGES, 0, o);
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        int to;
        if (k == DOWN) to = p->selected >= done ? 0 : p->selected + 1;
        else to = p->selected == cancel ? done - 1 : (p->selected + rows - 2) % (rows - 1);
        select_row(p, to, o);
        pointer_to(p, o);
    } else if (k == RIGHT && p->selected == cancel && p->page + 1 < PAGES) {
        turn(p, p->page + 1, 0, o);
    } else if (k == LEFT && p->selected == done && p->page > 0) {
        turn(p, p->page - 1, cancel_of(p->page - 1), o);
    } else if ((k == LEFT || k == RIGHT) && p->selected >= done) {
        o->pointer_off(o->ctx);
        select_row(p, p->selected == done ? cancel : done, o);
        pointer_to(p, o);
    } else if (k == LEFT || k == RIGHT) {
        change(p, e, unlock, k == RIGHT ? 1 : -1, o);
    }
}

int extras_menu_run(PortExtras *e, int16_t *unlock, int32_t bank, const ExtrasMenuOps *o)
{
    const PortExtras input = *e;
    const int16_t input_unlock = *unlock;
    Page p = { 0, ROW_MIRROR, 1, 0, 0, 0, -1 };   /* not released: the click that opened it may still be down */
    o->text_styles(o->ctx);
    pointer_to(&p, o);
    while (p.result < 0) {
        follow(&p, o);
        if (p.redraw) draw_page(&p, e, *unlock, bank, o);
        if (!p.armed) {
            p.rest += o->ticks(o->ctx);
            if ((int16_t)p.rest > REST) p.armed = 1;
        }
        const int32_t buttons = o->pointer_buttons(o->ctx);
        if (buttons == 0) p.released = 1;
        o->pointer_activate_if_moved(o->ctx);
        const uint16_t k = (uint16_t)o->key(o->ctx);
        const int clicked = buttons == LEFT_BUTTON && p.released;
        if ((k == SPACE || k == ENTER || clicked) && p.armed) choose(&p, e, unlock, o);
        else if (k != 0 && p.armed) step(&p, k, e, unlock, o);
        if (clicked) p.released = 0;
        if (k != 0 || clicked) p.armed = 0, p.rest = 0;
        o->music(o->ctx);
    }
    if (p.result != 1) *e = input, *unlock = input_unlock;
    return p.result;
}
