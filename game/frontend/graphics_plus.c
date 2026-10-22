/* port: the graphics screen's additions; see graphics_plus.h. The second page is its own loop over the same ops as
 * graphics_menu.c (00423d28) and behaves like it: the arrows move, Space / Enter / the left button choose, Escape
 * cancels, the idle deadline and the right button leave. */
#include "game/frontend/graphics_plus.h"

#include "core/formats/fsh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, PAGE_UP = 0x4900, LEFT = 0x4b00, RIGHT = 0x4d00,
       DOWN = 0x5000, PAGE_DOWN = 0x5100, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x2ee0, REST = 0x19, WIDE = 0x280 };
enum { ROW_DONE = PLUS_SETTINGS, ROW_CANCEL, ROWS };
enum { STYLE_CREAM = 0, STYLE_YELLOW = 1, LABEL_X = 8, VALUE_RIGHT = 493, NAV_CENTRE = 320, NAV_Y = 444 };
enum { FOV_MIN = -10, FOV_MAX = 20, FOV_STEP = 5 };
/* the darkened panel the second page's text sits on, over the original rows */
enum { PANEL_X0 = 4, PANEL_Y0 = 96, PANEL_X1 = 635, PANEL_Y1 = 404 };

/* the rows' place is the original's first five items': where the pointer goes and the bands it is asked about, then
 * Done and Cancel (00423c10 / 00423c20, 00463f80's calls) */
static const int16_t POINTER_X[ROWS] = { 230, 230, 230, 230, 230, 60, 565 };
static const int16_t POINTER_Y[ROWS] = { 116, 169, 220, 270, 325, 445, 445 };
static const int16_t BAND_LEFT[ROWS] = { 0, 0, 0, 0, 0, 0, 0x145 };
static const int16_t BAND_RIGHT[ROWS] = { WIDE, WIDE, WIDE, WIDE, WIDE, 0x140, WIDE };
static const int16_t BAND_TOP[ROWS] = { 0, 0x91, 0xc6, 0xf7, 0x12c, 0x1a4, 0x1a4 };
static const int16_t BAND_BOTTOM[ROWS] = { 0x8c, 0xc1, 0xf2, 0x127, 0x15c, 0x1e0, 0x1df };
/* the label art's top (the original rows 106, 159, 210, 315 and the view's 262) the text lines up with */
static const int16_t TEXT_Y[PLUS_SETTINGS] = { 106, 159, 210, 262, 315 };
static const char *const LABEL[PLUS_SETTINGS] = { "Screen format", "Refresh rate", "Scaling", "Full screen",
                                                  "Field of view" };
static const char *const FORMAT_TEXT[3] = { "4:3", "16:9", "21:9" };
static const int REFRESH[3] = { 60, 120, 144 };

static int wrap(int value, int direction, int count)
{
    return (value + direction + count) % count;
}

void graphics_plus_step(PortSettings *s, int setting, int direction, ScreenFormat fullscreen_format)
{
    switch (setting) {
    case PLUS_FORMAT:
        s->format = (ScreenFormat)wrap((int)s->format, direction, 3);
        break;
    case PLUS_REFRESH: {
        int at = 0;
        for (int i = 0; i < 3; i++)
            if (REFRESH[i] == s->refresh_hz) at = i;
        s->refresh_hz = REFRESH[wrap(at, direction, 3)];
        break;
    }
    case PLUS_SCALE:
        s->scale_integer ^= 1;
        break;
    case PLUS_FULLSCREEN:
        s->fullscreen ^= 1;
        if (s->fullscreen) s->format = fullscreen_format;
        break;
    case PLUS_FOV: {
        const int steps = (FOV_MAX - FOV_MIN) / FOV_STEP + 1;
        s->fov_adjust = FOV_MIN + FOV_STEP * wrap((s->fov_adjust - FOV_MIN) / FOV_STEP, direction, steps);
        break;
    }
    default:
        break;
    }
}

void graphics_plus_value(const PortSettings *s, int setting, char out[16])
{
    switch (setting) {
    case PLUS_FORMAT: snprintf(out, 16, "%s", FORMAT_TEXT[s->format]); break;
    case PLUS_REFRESH: snprintf(out, 16, "%d Hz", s->refresh_hz); break;
    case PLUS_SCALE: snprintf(out, 16, "%s", s->scale_integer ? "Whole" : "Fill"); break;
    case PLUS_FULLSCREEN: snprintf(out, 16, "%s", s->fullscreen ? "On" : "Off"); break;
    case PLUS_FOV: snprintf(out, 16, s->fov_adjust == 0 ? "0" : "%+d", s->fov_adjust); break;
    default: out[0] = '\0'; break;
    }
}

void graphics_plus_view_step(int16_t *view, int *ultra)
{
    enum { ULTRA = 3, VALUES = 4 };
    const int now = *ultra && *view == 0 ? ULTRA : *view;
    const int next = (now + 1) % VALUES;
    *view = (int16_t)(next == ULTRA ? 0 : next);
    *ultra = next == ULTRA;
}

static void text_right(const GraphicsMenuOps *o, int lit, const char *text, int right, int y)
{
    o->plus_text(o->ctx, lit ? STYLE_YELLOW : STYLE_CREAM, text, right - o->plus_text_width(o->ctx, text), y);
}

/* Ultra sits where the original puts Far's art (96x28 at (408, 261)). GRAPHICS.QFS has no "ultra" art; vanl / vand are
 * the LABEL column (195x25 at (5, 262)). Build a runtime FSH on the stack holding the bgnd sub-region 408..503,
 * 261..288 (the same 96x28 cell farl/fard occupy) — then blit_own at it. The contents are the live bgnd, so the
 * burnt-in screen art lives behind Ultra without an extra tile. */
enum { ULTRA_X = 408, ULTRA_Y = 261, ULTRA_W = 96, ULTRA_H = 28 };
static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_i16(uint8_t *p, int16_t v) { put_u16(p, (uint16_t)v); }

/* FSH wire layout at the shape pointer: type(1) + block_size(3 LE24) + width(2 LE) + height(2 LE) + misc[4](4x2 LE). */
static int bgnd_dims(const uint8_t *bgnd, uint16_t *w, uint16_t *h)
{
    if (bgnd == NULL) return 0;
    *w = (uint16_t)(bgnd[4] | ((uint16_t)bgnd[5] << 8));
    *h = (uint16_t)(bgnd[6] | ((uint16_t)bgnd[7] << 8));
    return 1;
}

void graphics_plus_ultra(const GraphicsMenuState *st, const GraphicsMenuOps *o, const uint8_t *rgb, int lit,
                         int32_t bank)
{
    (void)st;
    (void)rgb;
    const uint8_t *const bgnd = o->shape(o->ctx, bank, "bgnd");
    /* GRAPHICS.QFS's bgnd is 640x480 indexed pixels. The runtime FSH copies a 96x28 cell from it; the cell only
     * lies inside bgnd when its width and height reach at least 408 + 96 = 504 wide, 261 + 28 = 289 tall. */
    uint16_t bw = 0, bh = 0;
    if (!bgnd_dims(bgnd, &bw, &bh) || bw < ULTRA_X + ULTRA_W || bh < ULTRA_Y + ULTRA_H) {
        text_right(o, lit, "Ultra", VALUE_RIGHT, 262);
        return;
    }
    uint8_t cell[16 + ULTRA_W * ULTRA_H];
    memset(cell, 0, sizeof cell);
    cell[0] = FSH_TYPE_BITMAP;
    const uint32_t block = ULTRA_W * ULTRA_H;
    cell[1] = (uint8_t)(block); cell[2] = (uint8_t)(block >> 8); cell[3] = (uint8_t)(block >> 16);
    put_u16(cell + 4, ULTRA_W);
    put_u16(cell + 6, ULTRA_H);
    put_i16(cell + 8, 0);
    put_i16(cell + 10, 0);
    put_i16(cell + 12, ULTRA_X);
    put_i16(cell + 14, ULTRA_Y);
    const uint8_t *const src = bgnd + 16;
    for (int row = 0; row < ULTRA_H; row++)
        memcpy(cell + 16 + row * ULTRA_W, src + (ULTRA_Y + row) * (int)bw + ULTRA_X, ULTRA_W);
    o->blit_own(o->ctx, cell);
    text_right(o, lit, "Ultra", VALUE_RIGHT, 262);
}

void graphics_plus_nav(const GraphicsMenuState *st, const GraphicsMenuOps *o, int page)
{
    (void)st;
    const char *text = page == 0 ? "1/2 >" : "< 2/2";
    o->plus_text(o->ctx, STYLE_CREAM, text, NAV_CENTRE - o->plus_text_width(o->ctx, text) / 2, NAV_Y);
}

int graphics_plus_nav_hit(const GraphicsMenuOps *o)
{
    return o->pointer_in_rect(o->ctx, PLUS_NAV_X0, PLUS_NAV_X1, PLUS_NAV_Y0, PLUS_NAV_Y1);
}

typedef struct Page {
    int16_t selected;
    int redraw, armed, released;
    int32_t rest;
    int result;   /* a PlusPageResult once it is left, -1 until then */
} Page;

static void move_to(int row, const GraphicsMenuOps *o)
{
    o->pointer_move_to(o->ctx, POINTER_X[row], POINTER_Y[row]);
}

static void select_row(Page *p, int row, const GraphicsMenuOps *o)
{
    if (p->selected == row) return;
    p->selected = (int16_t)row;
    p->redraw = 1;
    o->sound(o->ctx, MOVED);
}

/* the first band the pointer is in becomes the selection; over the page indicator it is none */
static void follow(Page *p, const GraphicsMenuOps *o)
{
    if (graphics_plus_nav_hit(o)) return;
    for (int row = 0; row < ROWS; row++)
        if (o->pointer_in_rect(o->ctx, BAND_LEFT[row], BAND_RIGHT[row], BAND_TOP[row], BAND_BOTTOM[row])) {
            select_row(p, row, o);
            return;
        }
}

static void draw(Page *p, const GraphicsMenuState *st, const GraphicsMenuOps *o, int32_t bank, const uint8_t *rgb)
{
    char value[16];
    o->select(o->ctx, *st->screen);
    o->blit_own(o->ctx, o->shape(o->ctx, bank, "bgnd"));
    o->plus_panel(o->ctx, rgb, PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, 1);
    for (int row = 0; row < PLUS_SETTINGS; row++) {
        const int lit = p->selected == row;
        graphics_plus_value(st->plus->port, row, value);
        o->plus_text(o->ctx, lit ? STYLE_YELLOW : STYLE_CREAM, LABEL[row], LABEL_X, TEXT_Y[row]);
        text_right(o, lit, value, VALUE_RIGHT, TEXT_Y[row]);
    }
    if (p->selected == ROW_DONE) o->blit_own(o->ctx, o->shape(o->ctx, bank, "donl"));
    if (p->selected == ROW_CANCEL) o->blit_own(o->ctx, o->shape(o->ctx, bank, "canl"));
    graphics_plus_nav(st, o, 1);
    o->select_default(o->ctx);
    o->pointer_hide(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, *st->screen));
    o->unlock(o->ctx);
    o->flip(o->ctx);
    o->pointer_show(o->ctx);
    o->deadline_set(o->ctx, WAIT);
    p->redraw = 0;
}

/* Space, Enter or the left button */
static void choose(Page *p, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    o->pointer_click(o->ctx);
    if (p->armed) {
        o->sound(o->ctx, CHOSEN);
        if (p->selected < PLUS_SETTINGS) {
            graphics_plus_step(st->plus->port, p->selected, 1, st->plus->fullscreen_format);
            p->redraw = 1;
        } else {
            p->result = p->selected == ROW_DONE ? PLUS_PAGE_DONE : PLUS_PAGE_CANCEL;
        }
    }
    o->ticks(o->ctx);
    p->released = p->armed = 0;
    p->rest = 0;
}

/* a key other than the choosing ones */
static void step(Page *p, uint16_t k, const GraphicsMenuState *st, const GraphicsMenuOps *o)
{
    if (k == ESCAPE) {
        p->result = PLUS_PAGE_CANCEL;
        o->sound(o->ctx, CHOSEN);
    } else if (k == PAGE_UP || k == PAGE_DOWN) {
        p->result = PLUS_PAGE_BACK;
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        const int from = p->selected == ROW_CANCEL ? ROW_DONE : p->selected;
        const int to = k == DOWN ? (from + 1) % ROW_CANCEL : (from + ROW_CANCEL - 1) % ROW_CANCEL;
        select_row(p, to, o);
        move_to(p->selected, o);
    } else if (k == LEFT || k == RIGHT) {
        if (p->selected >= ROW_DONE) {
            o->pointer_off(o->ctx);
            select_row(p, p->selected == ROW_DONE ? ROW_CANCEL : ROW_DONE, o);
            move_to(p->selected, o);
        } else {
            graphics_plus_step(st->plus->port, p->selected, k == RIGHT ? 1 : -1, st->plus->fullscreen_format);
            o->sound(o->ctx, CHOSEN);
            p->redraw = 1;
        }
    }
    o->ticks(o->ctx);
    p->armed = 0;
    p->rest = 0;
}

PlusPageResult graphics_plus_page(const GraphicsMenuState *st, const GraphicsMenuOps *o, int32_t bank,
                                  const uint8_t *rgb)
{
    Page p = { 0, 1, 0, *st->buttons == 0, 0, -1 };   /* not armed: the key that turned the page may still be down */
    move_to(p.selected, o);
    do {
        follow(&p, o);
        if (p.redraw) draw(&p, st, o, bank, rgb);
        if (!p.armed) {
            p.rest += o->ticks(o->ctx);
            if ((int16_t)p.rest > REST) p.armed = 1;
        }
        if (*st->pointer_active) *st->buttons = (int16_t)o->pointer_buttons(o->ctx);
        if (*st->buttons == 0) p.released = 1;
        o->pointer_activate_if_moved(o->ctx);
        const uint16_t k = (uint16_t)o->key(o->ctx);
        if (*st->buttons == LEFT_BUTTON && p.released && graphics_plus_nav_hit(o)) {
            p.released = 0;
            p.result = PLUS_PAGE_BACK;
        } else if (k == SPACE || k == ENTER || (*st->buttons == LEFT_BUTTON && p.released)) {
            choose(&p, st, o);
        } else if (k == 0) {
            p.armed = 1;
        } else if (p.armed) {
            step(&p, k, st, o);
        }
        o->music(o->ctx);
        if (*st->net_off == 0) o->net_sync(o->ctx);
        if (p.result < 0 && (o->deadline_passed(o->ctx) || (*st->buttons == RIGHT_BUTTON && p.released) ||
                             *st->quit == 1)) {
            o->sound(o->ctx, CHOSEN);
            p.result = PLUS_PAGE_DONE;
        }
    } while (p.result < 0);
    return (PlusPageResult)p.result;
}
