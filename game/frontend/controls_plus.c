/* port: the controls screen's additions; see controls_plus.h. Page 2 lists the keyboard's driving keys, page 3 the
 * game controller's actions with their sources, page 4 its dead zone, sensitivity, force feedback and "Defaults"; the
 * pages 2..4 say under the title which device they set. Each page is a loop over the same ops as
 * controls_menu.c (00413cb0) and behaves like it: the arrows move, Space / Enter / the left button choose, Escape
 * cancels, the idle deadline and the right button leave. */
#include "game/frontend/controls_plus.h"

#include "game/frontend/page_arrows.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       INSERT = 0x5200, DELETE = 0x5300, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x2ee0, CAPTURE_WAIT = 500, REST = 0x19, WIDE = 0x280 };
enum { STYLE_CREAM = 0, STYLE_YELLOW = 1, STYLE_RED = 2, LABEL_X = 16, VALUE_RIGHT = 600, NAV_CENTRE = 320, STEP = 5 };
enum { PAGES = 4, KEYBOARD_PAGE = 1, ACTION_PAGE = 2, TUNE_PAGE = 3 };
/* the device each page is for, under the title */
enum { SUBTITLE_RIGHT = 600, SUBTITLE_Y = 60 };   /* on the title's line, to its right: the rows' right edge */
static const char *const SUBTITLE[PAGES - 1] = { "Keyboard", "Game controller", "Game controller" };
/* the title where the configuration screens' are (extras_menu.c's and graphics_own.c's TITLE_X, TITLE_Y) */
enum { TITLE_X = 8, TITLE_Y = 56 };
static const char *const TITLE = "Controls Setup";   /* title case: the title art's letters are the CD's own */
/* the rows: a line per row and per group header, 21 apart, a group after the first 8 lower; the second group's header
 * and rows end at y 402, so the page stays between the title's band and the buttons' (96..430) */
enum { ROW_TOP = 100, ROW_PITCH = 21, ROW_TALL = 19, GROUP_GAP = 8, DEFAULTS = TUNE_COUNT };
/* the tuning page's rows are as far apart as its '%' needs: graver18's reaches y 5..21 of its line, and the smooth draw
 * keeps its shadow and the background's cover (own_plus_text, 6 of room) below that, which the next row's cover
 * would cut off at 21 apart. The page has only five lines, so it has the room. */
enum { TUNE_PITCH = 28 };
/* each page's two groups: the rows in each, and their headers ("" none: the second group of page 3 is Defaults alone).
 * Indexed by page - KEYBOARD_PAGE. */
static const int GROUP_ROWS[PAGES - 1][2] = { { 6, 4 }, { 9, 4 }, { 3, 1 } };
static const char *const GROUP_HEADER[PAGES - 1][2] = { { "Driving", "Other" }, { "Driving", "Other" },
                                                          { "Tuning", NULL } };
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

/* the page indicator's words: over the clean background on pages 2 and 3 (put back under them), plain on page 1, which is
 * the exe's own picture */
static PageNavOps nav_ops(const ControlsMenuOps *o, int page)
{
    const int smooth = page > 0 && o->plus_text_smooth != NULL;
    const PageNavOps n = { o->ctx, smooth ? o->plus_text_smooth : o->plus_text, o->plus_text_width,
                           o->pointer_in_rect, o->plus_arrow_size, o->plus_arrow };
    return n;
}

void controls_plus_nav(const ControlsMenuOps *o, int page, int lit)
{
    const PageNavOps nav = nav_ops(o, page);
    page_nav_draw(&nav, page, PAGES, lit);
}

int controls_plus_nav_lit(const ControlsMenuOps *o, int page)
{
    const PageNavOps nav = nav_ops(o, page);
    return page_nav_lit(&nav, page, PAGES);
}

int controls_plus_nav_hit(const ControlsMenuOps *o)
{
    return o->pointer_in_rect(o->ctx, CONTROLS_NAV_X0, CONTROLS_NAV_X1, CONTROLS_NAV_Y0, CONTROLS_NAV_Y1);
}

/* the clean configuration background, composed once as the pages open (plus_bgnd); `ready` 0 when there is none, and
 * the exe's own picture is what shows */
typedef struct Bgnd {
    int ready;
    uint8_t pixels[CONTROLS_BGND_W * CONTROLS_BGND_H];
    uint8_t palette[768];
} Bgnd;

typedef struct Run {
    int page;                /* KEYBOARD_PAGE, ACTION_PAGE or TUNE_PAGE */
    int selected;
    int redraw, armed, released;
    int32_t rest;
    int capturing;           /* the row whose value is asked for, -1 for none */
    int have_pad;            /* a pad or wheel is plugged in (while a source is asked for) */
    int gamepad;             /* the device's numbers are SDL_Gamepad's (or none is plugged in) */
    int result;              /* a ControlsPageResult once it is left, -1 until then */
    int32_t bank;            /* the controls' own art: the picture when there is no clean background */
    int32_t options;         /* option/options.qfs: DONE and CANCEL's lit and dark art */
    const Bgnd *bgnd;
    int palette_on;          /* the clean background's palette put on the display, once: the pages share it */
    int nav_lit;             /* the page indicator's triangle the pointer is on, 0 none */
    int key_defaults[PKEY_COUNT];   /* config.dat's keys (the exe's page 1), what the keyboard has until it is the player's */
    const char *notice;      /* a key refused in the capture, shown in the row */
} Run;

/* the rows before Done: the keyboard's keys and Defaults, the actions, the tuning rows and Defaults */
static int rows(const Run *r)
{
    return r->page == KEYBOARD_PAGE ? PKEY_COUNT + 1 : r->page == ACTION_PAGE ? ACT_COUNT : TUNE_COUNT + 1;
}
static int done_row(const Run *r) { return rows(r); }
static int cancel_row(const Run *r) { return rows(r) + 1; }

/* the group a row is in, of the page's two */
static int group_of(int page, int row) { return row < GROUP_ROWS[page][0] ? 0 : 1; }

/* the line a group's header takes (or its first row, when it has none): the lines before it are the earlier groups' */
static int group_line(int page, int group)
{
    return group == 0 ? 0 : (GROUP_HEADER[page][0] != NULL) + GROUP_ROWS[page][0];
}

/* a group after the first is GROUP_GAP lower than the lines above it */
static int pitch_of(int page) { return page == TUNE_PAGE - KEYBOARD_PAGE ? TUNE_PITCH : ROW_PITCH; }
static int line_y(int page, int line, int group) { return ROW_TOP + line * pitch_of(page) + (group > 0 ? GROUP_GAP : 0); }

static int row_y(const Run *r, int row)
{
    const int page = r->page - KEYBOARD_PAGE, group = group_of(page, row);
    const int first = group == 0 ? 0 : GROUP_ROWS[page][0];
    const int line = group_line(page, group) + (GROUP_HEADER[page][group] != NULL) + (row - first);
    return line_y(page, line, group);
}

static int header_y(int page, int group) { return line_y(page, group_line(page, group), group); }

static void move_to(const Run *r, int row, const ControlsMenuOps *o)
{
    if (row >= done_row(r)) o->pointer_move_to(o->ctx, row == done_row(r) ? DONE_X : CANCEL_X, ITEMS_Y);
    else o->pointer_move_to(o->ctx, 200, row_y(r, row) + ROW_TALL / 2);
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
    for (int row = 0; row < rows(r); row++) {
        const int top = row_y(r, row) - 2;
        if (o->pointer_in_rect(o->ctx, 0, WIDE, top, top + pitch_of(r->page - KEYBOARD_PAGE))) {
            select_row(r, row, o);
            return;
        }
    }
}

/* the words go through the draw that puts the clean background back under them (own_plus_text), since the page is
 * drawn over it; with no smooth op the plain one */
static void words(const ControlsMenuOps *o, int32_t style, const char *text, int32_t x, int32_t y)
{
    if (o->plus_text_smooth != NULL) o->plus_text_smooth(o->ctx, style, text, x, y);
    else o->plus_text(o->ctx, style, text, x, y);
}

static void text_right(const ControlsMenuOps *o, int lit, const char *text, int right, int y)
{
    words(o, lit ? STYLE_YELLOW : STYLE_CREAM, text, right - o->plus_text_width(o->ctx, text), y);
}

/* a group's header: as the other configuration screens draw theirs, red (plus_header), or a red row without the op */
static void header(const ControlsMenuOps *o, const char *text, int page, int group)
{
    const int y = header_y(page, group);
    if (o->plus_header != NULL) o->plus_header(o->ctx, text, LABEL_X, y, VALUE_RIGHT - LABEL_X);
    else words(o, STYLE_RED, text, LABEL_X, y);
}

/* a key's words on the keyboard page: "NOT SET" (drawn red) for none, else the platform's name for the key */
static void key_text(const Run *r, const ControlsMenuState *st, const ControlsMenuOps *o, PortKey key, char *out,
                     size_t size)
{
    const int code = port_controls_key(st->plus->port, r->key_defaults, key);
    snprintf(out, size, "%s", "NOT SET");
    if (code != 0 && o->plus_key_name != NULL) o->plus_key_name(o->ctx, code, out, size);
}

static void draw_row(Run *r, const ControlsMenuState *st, const ControlsMenuOps *o, int row)
{
    char value[64];
    const int lit = r->selected == row, y = row_y(r, row);
    if (r->page == KEYBOARD_PAGE) {
        if (row == PKEY_COUNT) {
            words(o, lit ? STYLE_YELLOW : STYLE_CREAM, "Defaults", LABEL_X, y);
            return;
        }
        words(o, lit ? STYLE_YELLOW : STYLE_CREAM, port_controls_key_name((PortKey)row), LABEL_X, y);
        if (r->capturing == row) snprintf(value, sizeof value, "%s", r->notice != NULL ? r->notice : "Press a key...");
        else key_text(r, st, o, (PortKey)row, value, sizeof value);
        if (r->capturing != row && port_controls_key(st->plus->port, r->key_defaults, (PortKey)row) == 0)
            words(o, STYLE_RED, value, VALUE_RIGHT - o->plus_text_width(o->ctx, value), y);   /* a control with no key */
        else text_right(o, lit, value, VALUE_RIGHT, y);
    } else if (r->page == ACTION_PAGE) {
        words(o, lit ? STYLE_YELLOW : STYLE_CREAM, control_map_action_name((PortAction)row), LABEL_X, y);
        if (r->capturing == row)
            snprintf(value, sizeof value, "%s", r->have_pad ? "Press a button or move an axis..."
                                                            : "No controller found");
        else control_map_binding_text(st->plus->port, (PortAction)row, r->gamepad, value, sizeof value);
        text_right(o, lit, value, VALUE_RIGHT, y);
    } else if (row < TUNE_COUNT) {
        words(o, lit ? STYLE_YELLOW : STYLE_CREAM, TUNE_LABEL[row], LABEL_X, y);
        controls_plus_tune_text(st->plus->port, row, value);
        text_right(o, lit, value, VALUE_RIGHT, y);
    } else {
        words(o, lit ? STYLE_YELLOW : STYLE_CREAM, "Defaults", LABEL_X, y);
    }
}

static void draw(Run *r, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    const int clean = r->bgnd->ready;
    o->select(o->ctx, *st->screen);
    if (clean) o->plus_bgnd_shape(o->ctx, r->bgnd->pixels, r->bgnd->palette);
    else o->blit_own(o->ctx, o->shape(o->ctx, r->bank, "bgnd"));
    const int page = r->page - KEYBOARD_PAGE;
    for (int group = 0; group < 2; group++)
        if (GROUP_HEADER[page][group] != NULL) header(o, GROUP_HEADER[page][group], page, group);
    for (int row = 0; row < rows(r); row++) draw_row(r, st, o, row);
    if (o->plus_title != NULL) o->plus_title(o->ctx, TITLE, TITLE_X, TITLE_Y);
    else words(o, STYLE_CREAM, TITLE, TITLE_X, TITLE_Y);
    const char *const device = SUBTITLE[r->page - KEYBOARD_PAGE];
    const int device_x = SUBTITLE_RIGHT - o->plus_text_width(o->ctx, device);
    if (o->plus_subtitle != NULL) o->plus_subtitle(o->ctx, device, device_x, SUBTITLE_Y);
    else words(o, STYLE_YELLOW, device, device_x, SUBTITLE_Y);
    /* DONE and CANCEL are the background's own words; their lit and dark art is Options' (OPTIONS.QFS's capitalised
     * names: shape_bank_find compares with memcmp) */
    if (r->options != 0) {
        const uint8_t *const done = o->shape(o->ctx, r->options, r->selected == done_row(r) ? "Donl" : "Dond");
        const uint8_t *const cancel = o->shape(o->ctx, r->options, r->selected == cancel_row(r) ? "Canl" : "Cand");
        if (done != NULL) o->blit_own(o->ctx, done);
        if (cancel != NULL) o->blit_own(o->ctx, cancel);
    }
    controls_plus_nav(o, r->page, r->nav_lit);
    o->select_default(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, *st->screen));
    /* the clean background's palette goes on the display once: fade the page the caller had out, put this one up on the
     * black display, and fade it in. The pages after share it, so they only flip. */
    if (clean && !r->palette_on) {
        o->fade_out(o->ctx, 0, 0x100);
        o->flip(o->ctx);
        o->fade_in(o->ctx, 0, 0x100, r->bgnd->palette);
        o->text_styles(o->ctx);
        r->palette_on = 1;
    } else {
        o->flip(o->ctx);
    }
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
        ControlSource gave_up = { SRC_NONE, 0, 0, 0 };   /* what the row had: the action that had `got` is given it */
        if (add && c->bind[act][0].kind != SRC_NONE) slot = 1;
        else base.bind[act][1] = (ControlSource){ SRC_NONE, 0, 0, 0 };
        if (slot == 0) gave_up = c->bind[act][0], base.bind[act][0] = (ControlSource){ SRC_NONE, 0, 0, 0 };
        *c = control_map_swap(&base, (PortAction)act, slot, got, gave_up);
        c->custom = 1;
        o->sound(o->ctx, CHOSEN);
    }
    r->redraw = 1;
}

/* a key row asked for a key: any key that is not the game's own (port_controls_key_reserved: refused, the row says so)
 * pressed after the capture began, Escape or CAPTURE_WAIT ticks of nothing leave the row as it was. A key another row
 * had is taken from it. */
static void capture_key(Run *r, PortKey key, const ControlsMenuState *st, const ControlsMenuOps *o)
{
    enum { FIRST_KEY = 1, LAST_KEY = 0x58 };
    uint8_t rest[LAST_KEY + 1] = { 0 };
    for (int code = FIRST_KEY; code <= LAST_KEY; code++) rest[code] = o->plus_key_down(o->ctx, code) != 0;
    int got = 0;
    r->capturing = (int)key;
    r->notice = NULL;
    draw(r, st, o);
    o->deadline_set(o->ctx, CAPTURE_WAIT);
    while (got == 0 && !o->deadline_passed(o->ctx)) {
        o->music(o->ctx);
        if ((uint16_t)o->key(o->ctx) == ESCAPE) break;
        for (int code = FIRST_KEY; code <= LAST_KEY && got == 0; code++) {
            const int down = o->plus_key_down(o->ctx, code) != 0;
            if (down && !rest[code] && !port_controls_key_reserved(code)) got = code;
            else if (down && !rest[code] && r->notice == NULL) r->notice = "Key not available", draw(r, st, o);
            rest[code] = down;
        }
    }
    drain(o);
    r->capturing = -1;
    r->notice = NULL;
    if (got != 0) {
        port_controls_set_key(st->plus->port, r->key_defaults, key, got);
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
        else if (r->page == KEYBOARD_PAGE && r->selected == PKEY_COUNT) {
            port_controls_reset_keys(st->plus->port);   /* config.dat's keys again */
            r->redraw = 1;
        } else if (r->page == KEYBOARD_PAGE) capture_key(r, (PortKey)r->selected, st, o);
        else if (r->page == ACTION_PAGE) capture(r, r->selected, 0, st, o);
        else if (r->selected == DEFAULTS) {
            PortControls defaults = port_controls_defaults();   /* custom 0: as without a controls.cfg */
            memcpy(defaults.key, st->plus->port->key, sizeof defaults.key);   /* the keyboard's are its own page's */
            defaults.keys_custom = st->plus->port->keys_custom;
            *st->plus->port = defaults;
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
        if (r->page == KEYBOARD_PAGE) r->result = CONTROLS_PAGE_BACK;
        else turn(r, r->page - 1, o);
    } else if (k == RIGHT) {
        turn(r, r->page + 1, o);
    } else if (r->page == KEYBOARD_PAGE && r->selected < PKEY_COUNT && k == DELETE) {
        port_controls_set_key(st->plus->port, r->key_defaults, (PortKey)r->selected, 0);   /* none */
        r->redraw = 1;
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
    else if (r->page == KEYBOARD_PAGE) r->result = CONTROLS_PAGE_BACK;
    else turn(r, r->page - 1, o);
}

ControlsPageResult controls_plus_run(const ControlsMenuState *st, const ControlsMenuOps *o, int32_t bank)
{
    static Bgnd bgnd;   /* 300 KB: not on the stack */
    RawPad pad;
    bgnd.ready = o->plus_bgnd != NULL && o->plus_bgnd_shape != NULL &&
                 o->plus_bgnd(o->ctx, bgnd.pixels, bgnd.palette) != 0;
    const int32_t options = o->load_art(o->ctx, "option/options.qfs");
    /* not armed: the key that turned the page may still be down */
    Run r = { .page = KEYBOARD_PAGE, .redraw = 1, .released = *st->buttons == 0, .capturing = -1, .gamepad = 1,
              .result = -1, .bank = bank, .options = options, .bgnd = &bgnd, .nav_lit = 0 };
    if (o->pad_raw(o->ctx, &pad)) r.gamepad = pad.gamepad;
    if (st->controls != NULL && st->german != NULL) port_controls_key_defaults(st->controls, *st->german, r.key_defaults);
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
    if (options != 0) o->free(o->ctx, options);
    return (ControlsPageResult)r.result;
}
