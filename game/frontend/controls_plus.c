/* port: the controls screen's additions; see controls_plus.h. Page 2 lists the actions with their sources, page 3 the
 * dead zone, the sensitivity, the force feedback and "Defaults". Each page is a loop over the same ops as
 * controls_menu.c (00413cb0) and behaves like it: the arrows move, Space / Enter / the left button choose, Escape
 * cancels, the idle deadline and the right button leave. */
#include "game/frontend/controls_plus.h"

#include "game/frontend/page_arrows.h"

#include <stdio.h>
#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       INSERT = 0x5200, DELETE = 0x5300, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x2ee0, CAPTURE_WAIT = 500, REST = 0x19, WIDE = 0x280 };
enum { STYLE_CREAM = 0, STYLE_YELLOW = 1, LABEL_X = 16, VALUE_RIGHT = 623, NAV_CENTRE = 320, NAV_Y = 444, STEP = 5 };
enum { PAGES = 3, ACTION_PAGE = 1, TUNE_PAGE = 2, HINT_Y = 408 };
/* the darkened panel the pages' text sits on, over the original rows */
enum { PANEL_X0 = 4, PANEL_Y0 = 96, PANEL_X1 = 635, PANEL_Y1 = 404 };
/* the rows: the actions' on page 2, the tuning rows' and "Defaults" on page 3 */
enum { ACTION_Y = 104, ACTION_H = 24, TUNE_Y = 128, TUNE_H = 52, DEFAULTS = TUNE_COUNT };
/* Done and Cancel as the original's (00413aa0, 00463f80's calls) */
enum { DONE_X = 50, CANCEL_X = 565, ITEMS_Y = 450 };

static const char *const TUNE_LABEL[TUNE_COUNT] = { "Dead zone", "Sensitivity", "Force feedback" };
static const int TUNE_MIN[TUNE_COUNT] = { 0, 50, 0 };
static const int TUNE_MAX[TUNE_COUNT] = { 50, 200, 100 };

static int *tune_field(PortControls *c, int which)
{
    return which == TUNE_DEADZONE ? &c->deadzone : which == TUNE_SENSITIVITY ? &c->sensitivity : &c->ffb_strength;
}

static int tune_value(const PortControls *c, int which)
{
    return which == TUNE_DEADZONE ? c->deadzone : which == TUNE_SENSITIVITY ? c->sensitivity : c->ffb_strength;
}

int controls_plus_tune(PortControls *c, int which, int direction)
{
    if (which < 0 || which >= TUNE_COUNT) return 0;
    int *v = tune_field(c, which);
    int next = *v + (direction < 0 ? -STEP : STEP);
    if (next < TUNE_MIN[which]) next = TUNE_MIN[which];
    if (next > TUNE_MAX[which]) next = TUNE_MAX[which];
    const int moved = next != *v;
    *v = next;
    if (moved) c->custom = 1;
    return moved;
}

void controls_plus_tune_text(const PortControls *c, int which, char out[16])
{
    out[0] = '\0';
    if (which >= 0 && which < TUNE_COUNT) snprintf(out, 16, "%d%%", tune_value(c, which));
}

static PageNavOps nav_ops(const ControlsMenuOps *o)
{
    const PageNavOps n = { o->ctx, o->plus_text, o->plus_text_width, o->pointer_in_rect, o->plus_arrow_size,
                           o->plus_arrow };
    return n;
}

void controls_plus_nav(const ControlsMenuOps *o, int page, int lit)
{
    const PageNavOps nav = nav_ops(o);
    page_nav_draw(&nav, page, PAGES, lit);
}

int controls_plus_nav_lit(const ControlsMenuOps *o, int page)
{
    const PageNavOps nav = nav_ops(o);
    return page_nav_lit(&nav, page, PAGES);
}

int controls_plus_nav_hit(const ControlsMenuOps *o)
{
    return o->pointer_in_rect(o->ctx, CONTROLS_NAV_X0, CONTROLS_NAV_X1, CONTROLS_NAV_Y0, CONTROLS_NAV_Y1);
}

typedef struct Run {
    int page;                /* ACTION_PAGE or TUNE_PAGE */
    int selected;
    int redraw, armed, released;
    int32_t rest;
    int capturing;           /* the row whose value is asked for, -1 for none */
    int have_pad;            /* a pad or wheel is plugged in (while a source is asked for) */
    int gamepad;             /* the device's numbers are SDL_Gamepad's (or none is plugged in) */
    int result;              /* a ControlsPageResult once it is left, -1 until then */
    int32_t bank;
    const uint8_t *rgb;
    int nav_lit;             /* the page indicator's triangle the pointer is on, 0 none */
} Run;

static int rows(const Run *r) { return r->page == ACTION_PAGE ? ACT_COUNT : TUNE_COUNT + 1; }   /* before Done */
static int done_row(const Run *r) { return rows(r); }
static int cancel_row(const Run *r) { return rows(r) + 1; }

static int row_top(const Run *r, int row)
{
    return r->page == ACTION_PAGE ? ACTION_Y + row * ACTION_H : TUNE_Y + row * TUNE_H;
}

static void move_to(const Run *r, int row, const ControlsMenuOps *o)
{
    if (row >= done_row(r)) o->pointer_move_to(o->ctx, row == done_row(r) ? DONE_X : CANCEL_X, ITEMS_Y);
    else o->pointer_move_to(o->ctx, 200, row_top(r, row) + ACTION_H / 2);
}

static void select_row(Run *r, int row, const ControlsMenuOps *o)
{
    if (r->selected == row) return;
    r->selected = row;
    r->redraw = 1;
    o->sound(o->ctx, MOVED);
}

/* the first band the pointer is in becomes the selection; over the page indicator it is none */
static void follow(Run *r, const ControlsMenuOps *o)
{
    const int lit = controls_plus_nav_lit(o, r->page);
    if (lit != r->nav_lit) r->nav_lit = lit, r->redraw = 1;
    if (controls_plus_nav_hit(o)) return;
    if (o->pointer_in_rect(o->ctx, 0x145, WIDE, 0x195, 0x1e0)) {
        select_row(r, cancel_row(r), o);
        return;
    }
    if (o->pointer_in_rect(o->ctx, 0, 0x140, 0x195, 0x1e0)) {
        select_row(r, done_row(r), o);
        return;
    }
    const int h = r->page == ACTION_PAGE ? ACTION_H : TUNE_H;
    for (int row = 0; row < rows(r); row++) {
        const int top = row_top(r, row) - 2;
        if (o->pointer_in_rect(o->ctx, 0, WIDE, top, top + h)) {
            select_row(r, row, o);
            return;
        }
    }
}

static void text_right(const ControlsMenuOps *o, int lit, const char *text, int right, int y)
{
    o->plus_text(o->ctx, lit ? STYLE_YELLOW : STYLE_CREAM, text, right - o->plus_text_width(o->ctx, text), y);
}

static void draw(Run *r, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    char value[64];
    o->select(o->ctx, *st->screen);
    o->blit_own(o->ctx, o->shape(o->ctx, r->bank, "bgnd"));
    o->plus_panel(o->ctx, r->rgb, PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, 1);
    for (int row = 0; row < rows(r); row++) {
        const int lit = r->selected == row, y = row_top(r, row);
        if (r->page == ACTION_PAGE) {
            o->plus_text(o->ctx, lit ? STYLE_YELLOW : STYLE_CREAM, control_map_action_name((PortAction)row), LABEL_X, y);
            if (r->capturing == row)
                snprintf(value, sizeof value, "%s", r->have_pad ? "Press a button or move an axis..."
                                                                : "No controller found");
            else control_map_binding_text(st->plus->port, (PortAction)row, r->gamepad, value, sizeof value);
            text_right(o, lit, value, VALUE_RIGHT, y);
        } else if (row < TUNE_COUNT) {
            o->plus_text(o->ctx, lit ? STYLE_YELLOW : STYLE_CREAM, TUNE_LABEL[row], LABEL_X, y);
            controls_plus_tune_text(st->plus->port, row, value);
            text_right(o, lit, value, VALUE_RIGHT, y);
        } else {
            o->plus_text(o->ctx, lit ? STYLE_YELLOW : STYLE_CREAM, "Defaults", LABEL_X, y);
        }
    }
    if (r->page == ACTION_PAGE) {
        const char *hint = "Enter: set   Insert: add   Delete: clear";
        o->plus_text(o->ctx, STYLE_CREAM, hint, NAV_CENTRE - o->plus_text_width(o->ctx, hint) / 2, HINT_Y);
    }
    if (r->selected == done_row(r)) o->blit_own(o->ctx, o->shape(o->ctx, r->bank, "donl"));
    if (r->selected == cancel_row(r)) o->blit_own(o->ctx, o->shape(o->ctx, r->bank, "canl"));
    controls_plus_nav(o, r->page, r->nav_lit);
    o->select_default(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, *st->screen));
    o->flip(o->ctx);
    o->pointer_show(o->ctx);
    o->deadline_set(o->ctx, WAIT);
    r->redraw = 0;
}

static void drain(const ControlsMenuOps *o)
{
    while (o->key(o->ctx) != 0) o->music(o->ctx);
}

/* an action's row asked for a source: Escape or CAPTURE_WAIT ticks of nothing leave it as it was. `add`: as a second
 * source (the row keeps its first); else it replaces both. */
static void capture(Run *r, int act, int add, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    RawPad rest, now;
    const int have = o->pad_raw(o->ctx, &rest) != 0;
    r->have_pad = have;
    ControlSource got = { SRC_NONE, 0, 0, 0 };
    r->capturing = act;
    draw(r, st, o);
    o->deadline_set(o->ctx, CAPTURE_WAIT);
    while (got.kind == SRC_NONE && !o->deadline_passed(o->ctx)) {
        o->music(o->ctx);
        if ((uint16_t)o->key(o->ctx) == ESCAPE) break;
        if (have && o->pad_raw(o->ctx, &now)) got = control_map_detect(&rest, &now, act == ACT_STEER);
    }
    drain(o);
    r->capturing = -1;
    if (got.kind != SRC_NONE) {
        PortControls *c = st->plus->port;
        PortControls base = *c;
        int slot = 0;
        if (add && c->bind[act][0].kind != SRC_NONE) slot = 1;
        else base.bind[act][1] = (ControlSource){ SRC_NONE, 0, 0, 0 };
        if (slot == 0) base.bind[act][0] = (ControlSource){ SRC_NONE, 0, 0, 0 };
        *c = control_map_assign(&base, (PortAction)act, slot, got);
        c->custom = 1;
        o->sound(o->ctx, CHOSEN);
    }
    r->redraw = 1;
}

static void clear_action(Run *r, int act, const ControlsMenuState *st)
{
    PortControls *c = st->plus->port;
    c->bind[act][0] = c->bind[act][1] = (ControlSource){ SRC_NONE, 0, 0, 0 };
    c->custom = 1;
    r->redraw = 1;
}

static void turn(Run *r, int to, const ControlsMenuOps *o)
{
    if (to < 1 || to > PAGES - 1) return;
    r->page = to;
    r->selected = 0;
    r->redraw = 1;
    move_to(r, 0, o);
    o->sound(o->ctx, MOVED);
}

/* Space, Enter or the left button */
static void choose(Run *r, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    o->pointer_click(o->ctx);
    if (r->armed) {
        o->sound(o->ctx, CHOSEN);
        if (r->selected == done_row(r)) r->result = CONTROLS_PAGE_DONE;
        else if (r->selected == cancel_row(r)) r->result = CONTROLS_PAGE_CANCEL;
        else if (r->page == ACTION_PAGE) capture(r, r->selected, 0, st, o);
        else if (r->selected == DEFAULTS) {
            *st->plus->port = port_controls_defaults();   /* custom 0: as without a controls.cfg */
            r->redraw = 1;
        } else {   /* a tuning row: one step up, from the top back to the bottom */
            if (!controls_plus_tune(st->plus->port, r->selected, 1)) {
                *tune_field(st->plus->port, r->selected) = TUNE_MIN[r->selected];
                st->plus->port->custom = 1;
            }
            r->redraw = 1;
        }
    }
    o->ticks(o->ctx);
    r->released = r->armed = 0;
    r->rest = 0;
}

/* a key other than the choosing ones */
static void step(Run *r, uint16_t k, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    const int on_item = r->selected >= done_row(r);
    if (k == ESCAPE) {
        r->result = CONTROLS_PAGE_CANCEL;
        o->sound(o->ctx, CHOSEN);
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        const int count = cancel_row(r);   /* the rows and Done; Cancel is reached from Done with Left / Right */
        const int from = r->selected == cancel_row(r) ? done_row(r) : r->selected;
        select_row(r, k == DOWN ? (from + 1) % count : (from + count - 1) % count, o);
        move_to(r, r->selected, o);
    } else if (on_item && (k == LEFT || k == RIGHT)) {
        o->pointer_off(o->ctx);
        select_row(r, r->selected == done_row(r) ? cancel_row(r) : done_row(r), o);
        move_to(r, r->selected, o);
    } else if (r->page == TUNE_PAGE && r->selected < TUNE_COUNT && (k == LEFT || k == RIGHT)) {
        controls_plus_tune(st->plus->port, r->selected, k == RIGHT ? 1 : -1);
        o->sound(o->ctx, CHOSEN);
        r->redraw = 1;
    } else if (k == LEFT) {
        if (r->page == ACTION_PAGE) r->result = CONTROLS_PAGE_BACK;
        else turn(r, r->page - 1, o);
    } else if (k == RIGHT) {
        turn(r, r->page + 1, o);
    } else if (r->page == ACTION_PAGE && r->selected < ACT_COUNT && (k == DELETE || k == INSERT)) {
        if (k == DELETE) clear_action(r, r->selected, st);
        else capture(r, r->selected, 1, st, o);
    }
    o->ticks(o->ctx);
    r->armed = 0;
    r->rest = 0;
}

/* the indicator clicked: its left half back, its right half on */
static void nav_click(Run *r, const ControlsMenuOps *o)
{
    const int back = o->pointer_in_rect(o->ctx, CONTROLS_NAV_X0, NAV_CENTRE, CONTROLS_NAV_Y0, CONTROLS_NAV_Y1);
    if (!back) turn(r, r->page + 1, o);
    else if (r->page == ACTION_PAGE) r->result = CONTROLS_PAGE_BACK;
    else turn(r, r->page - 1, o);
}

ControlsPageResult controls_plus_run(const ControlsMenuState *st, const ControlsMenuOps *o, int32_t bank,
                                     const uint8_t *rgb)
{
    RawPad pad;
    /* not armed: the key that turned the page may still be down */
    Run r = { ACTION_PAGE, 0, 1, 0, *st->buttons == 0, 0, -1, 0, 1, -1, bank, rgb, 0 };
    if (o->pad_raw(o->ctx, &pad)) r.gamepad = pad.gamepad;
    move_to(&r, r.selected, o);
    do {
        follow(&r, o);
        if (r.redraw) draw(&r, st, o);
        if (!r.armed) {
            r.rest += o->ticks(o->ctx);
            if ((int16_t)r.rest > REST) r.armed = 1;
        }
        if (*st->pointer_active) *st->buttons = (int16_t)o->pointer_buttons(o->ctx);
        if (*st->buttons == 0) r.released = 1;
        o->pointer_activate_if_moved(o->ctx);
        const uint16_t k = (uint16_t)o->key(o->ctx);
        if (*st->buttons == LEFT_BUTTON && r.released && controls_plus_nav_hit(o)) {
            r.released = 0;
            nav_click(&r, o);
        } else if (k == SPACE || k == ENTER || (*st->buttons == LEFT_BUTTON && r.released)) {
            choose(&r, st, o);
        } else if (k == 0) {
            r.armed = 1;
        } else if (r.armed) {
            step(&r, k, st, o);
        }
        o->music(o->ctx);
        if (*st->net_off == 0) o->net_sync(o->ctx);
        if (r.result < 0 && (o->deadline_passed(o->ctx) || (*st->buttons == RIGHT_BUTTON && r.released) ||
                             *st->quit == 1)) {
            o->sound(o->ctx, CHOSEN);
            r.result = CONTROLS_PAGE_DONE;
        }
    } while (r.result < 0);
    return (ControlsPageResult)r.result;
}
