/* port: the Graphics screen, the port's own; see graphics_own.h. Its loop is graphics_plus.c's second page over the
 * same ops as graphics_menu.c (00423d28): the arrows move, Space / Enter / the left button choose, Escape cancels,
 * the idle deadline and the right button leave. What is new is the background (the clean one, composed once at run
 * time), the title, and the nine rows, which are all text over it. */
#include "game/frontend/graphics_own.h"

#include "core/formats/fsh.h"

#include <stdio.h>
#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };
enum { MOVED = 0x47, CHOSEN = 0x48, WAIT = 0x2ee0, REST = 0x19 };
enum { STYLE_CREAM = 0, STYLE_YELLOW = 1 };
enum { FOV_MIN = -10, FOV_MAX = 20, FOV_STEP = 5 };
/* the title, where the burnt-in one was: right of the logo, on the row config_bgnd.c takes the title band from
 * (its BANDS[0], 110,58,640,95), so the letters land where the original's were. */
enum { TITLE_X = 120, TITLE_Y = 62 };
/* the rows' text needs no panel: the background behind them is the clean one, with nothing burnt into it */

static const char *const LABEL[GRAPHICS_OWN_SETTINGS] = { "Perspective Correction", "Texture Filter", "Scaling",
                                                          "View Distance", "Fog", "Field of view", "Screen format",
                                                          "Refresh rate", "Full screen" };
static const char *const TITLE = "GRAPHICS";
static const char *const FORMAT_TEXT[3] = { "4:3", "16:9", "21:9" };
static const int REFRESH[3] = { 60, 120, 144 };

/* The rows' place. The exe's own bands (00423c10 / 00423c20 and graphics_menu.c's copies of them) are 0..0x8c for
 * its first row and then about 53 apart, with Done and Cancel in 0x1a4..0x1e0. Nine settings do not fit at that
 * pitch -- 0x1e0 is the bottom of the screen -- so the first row keeps the exe's band and the other eight share the
 * 280 pixels between it and Done's, 35 each. The x's are the exe's: the wide 0x280 for the settings, and its 0x140 /
 * 0x145 corners for the two buttons, which are the background's own words (config_bgnd.c's CORNERS). */
enum { WIDE = 0x280 };
static const int16_t POINTER_X[GRAPHICS_OWN_ROWS] = { 230, 230, 230, 230, 230, 230, 230, 230, 230, 60, 565 };
static const int16_t POINTER_Y[GRAPHICS_OWN_ROWS] = { 116, 160, 195, 230, 265, 300, 335, 370, 405, 445, 445 };
static const int16_t BAND_LEFT[GRAPHICS_OWN_ROWS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x145 };
static const int16_t BAND_RIGHT[GRAPHICS_OWN_ROWS] = { WIDE, WIDE, WIDE, WIDE, WIDE, WIDE, WIDE, WIDE, WIDE, 0x140, WIDE };
static const int16_t BAND_TOP[GRAPHICS_OWN_ROWS] = { 0x0, 0x8c, 0xaf, 0xd2, 0xf5, 0x118, 0x13b, 0x15e, 0x181,
                                                     0x1a4, 0x1a4 };
static const int16_t BAND_BOTTOM[GRAPHICS_OWN_ROWS] = { 0x8c, 0xaf, 0xd2, 0xf5, 0x118, 0x13b, 0x15e, 0x181, 0x1a4,
                                                        0x1e0, 0x1df };
/* the labels' y, as graphics_menu.c's TEXT_Y is: the first row where the exe's own put it, the rest ten below their
 * band's top, and ten below every label the pointer, the exe's own 106 / 159 / 210 ... being 116 / 169 / 220 */
static const int16_t TEXT_Y[GRAPHICS_OWN_ROWS] = { 106, 150, 185, 220, 255, 290, 325, 360, 395, 438, 438 };

static int wrap(int value, int direction, int count)
{
    return (value + direction + count) % count;
}

/* config.dat's view distance short (00524e75), read and written as the original reads it, unaligned */
static int16_t view_of(const GraphicsMenuState *st)
{
    int16_t v;
    memcpy(&v, st->config + GRAPHICS_VIEW, sizeof v);
    return v;
}
static void view_to(const GraphicsMenuState *st, int16_t v)
{
    memcpy(st->config + GRAPHICS_VIEW, &v, sizeof v);
}

void graphics_own_view_step(int16_t *view, int *ultra)
{
    enum { ULTRA = 3, VALUES = 4 };
    const int now = *ultra && *view == 0 ? ULTRA : *view;
    const int next = (now + 1) % VALUES;
    *view = (int16_t)(next == ULTRA ? 0 : next);
    *ultra = next == ULTRA;
}

void graphics_own_view_value(int16_t view, int ultra, char out[16])
{
    static const char *const NAMES[3] = { "Far", "Medium", "Close" };
    const char *const name = ultra && view == 0 ? "Ultra" : NAMES[view >= 0 && view < 3 ? view : 0];
    snprintf(out, 16, "%s", name);
}

void graphics_own_step(PortSettings *s, int setting, int direction, ScreenFormat fullscreen_format)
{
    switch (setting) {
    case GRAPHICS_OWN_PERSPECTIVE:
        s->perspective ^= 1;
        break;
    case GRAPHICS_OWN_FILTER:
        s->texture_filter ^= 1;
        break;
    case GRAPHICS_OWN_SCALING:
        s->scale_integer ^= 1;
        break;
    case GRAPHICS_OWN_FOG:
        s->fog ^= 1;
        break;
    case GRAPHICS_OWN_FORMAT:
        s->format = (ScreenFormat)wrap((int)s->format, direction, 3);
        break;
    case GRAPHICS_OWN_REFRESH: {
        int at = 0;
        for (int i = 0; i < 3; i++)
            if (REFRESH[i] == s->refresh_hz) at = i;
        s->refresh_hz = REFRESH[wrap(at, direction, 3)];
        break;
    }
    case GRAPHICS_OWN_FULLSCREEN:
        s->fullscreen ^= 1;
        if (s->fullscreen) s->format = fullscreen_format;
        break;
    case GRAPHICS_OWN_FOV: {
        const int steps = (FOV_MAX - FOV_MIN) / FOV_STEP + 1;
        s->fov_adjust = FOV_MIN + FOV_STEP * wrap((s->fov_adjust - FOV_MIN) / FOV_STEP, direction, steps);
        break;
    }
    default:
        break;   /* the view distance steps through config.dat's own short, not here */
    }
}

/* the eight rows that live in port.cfg. The view distance is read from config.dat instead, by the caller. */
void graphics_own_value(const PortSettings *s, int setting, char out[16])
{
    switch (setting) {
    case GRAPHICS_OWN_PERSPECTIVE: snprintf(out, 16, "%s", s->perspective ? "On" : "Off"); break;
    case GRAPHICS_OWN_FILTER: snprintf(out, 16, "%s", s->texture_filter ? "On" : "Off"); break;
    case GRAPHICS_OWN_SCALING: snprintf(out, 16, "%s", s->scale_integer ? "Whole" : "Fill"); break;
    case GRAPHICS_OWN_FOG: snprintf(out, 16, "%s", s->fog ? "On" : "Off"); break;
    case GRAPHICS_OWN_FOV: snprintf(out, 16, s->fov_adjust == 0 ? "0" : "%+d", s->fov_adjust); break;
    case GRAPHICS_OWN_FORMAT: snprintf(out, 16, "%s", FORMAT_TEXT[s->format]); break;
    case GRAPHICS_OWN_REFRESH: snprintf(out, 16, "%d Hz", s->refresh_hz); break;
    case GRAPHICS_OWN_FULLSCREEN: snprintf(out, 16, "%s", s->fullscreen ? "On" : "Off"); break;
    default: out[0] = '\0'; break;
    }
}

typedef struct Screen {
    const GraphicsMenuState *st;
    int16_t selected;
    int redraw, armed, released, result;   /* result: -1 while the screen is up, else 1 Done / 0 Cancel */
    int32_t rest;
    uint8_t pixels[GRAPHICS_OWN_W * GRAPHICS_OWN_H];
    uint8_t palette[768];
    int bgnd_ready;   /* the background asked for once; composing it takes about half a second */
    int palette_on;   /* the palette put on the display, once: it is the same one on every redraw */
    uint8_t screen_pal[768];   /* the palette_set destination, kept apart from the one it reads */
} Screen;

/* The clean configuration background, composed at run time from the CD and kept: config_bgnd_load reads the ten
 * screens it is built from, and the screen redraws on every move. `plus_bgnd` is the glue's; with none (a screen
 * that only wants the loop) the background is left as the caller had it. */
static void background(Screen *s, const GraphicsMenuOps *o)
{
    if (s->bgnd_ready || o->plus_bgnd == NULL) return;
    s->bgnd_ready = 1;
    if (!o->plus_bgnd(s->pixels, s->palette))
        fprintf(stderr, "graphics: no clean configuration background, the burnt-in one stays\n");
}

static void text_right(const GraphicsMenuOps *o, int lit, const char *text, int right, int y)
{
    o->plus_text(o->ctx, lit ? STYLE_YELLOW : STYLE_CREAM, text, right - o->plus_text_width(o->ctx, text), y);
}

static void move_to(int row, const GraphicsMenuOps *o)
{
    o->pointer_move_to(o->ctx, POINTER_X[row], POINTER_Y[row]);
}

static void select_row(Screen *s, int row, const GraphicsMenuOps *o)
{
    if (s->selected == row) return;
    s->selected = (int16_t)row;
    s->redraw = 1;
    o->sound(o->ctx, MOVED);
}

/* the first band the pointer is in becomes the selection */
static void follow(Screen *s, const GraphicsMenuOps *o)
{
    for (int row = 0; row < GRAPHICS_OWN_ROWS; row++)
        if (o->pointer_in_rect(o->ctx, BAND_LEFT[row], BAND_RIGHT[row], BAND_TOP[row], BAND_BOTTOM[row])) {
            select_row(s, row, o);
            return;
        }
}

static int row_y(int row)
{
    return TEXT_Y[row];
}

/* one row's text: the label at the column's left edge, its value at the right, the selection lit */
static void row_text(Screen *s, int row, const GraphicsMenuOps *o)
{
    char value[16];
    const int lit = s->selected == row;
    if (row == GRAPHICS_OWN_VIEW)
        graphics_own_view_value(view_of(s->st), s->st->plus->port->view_ultra, value);
    else
        graphics_own_value(s->st->plus->port, row, value);
    o->plus_text(o->ctx, lit ? STYLE_YELLOW : STYLE_CREAM, LABEL[row], GRAPHICS_OWN_LABEL_X, row_y(row));
    text_right(o, lit, value, GRAPHICS_OWN_VALUE_RIGHT, row_y(row));
}

/* the clean background is a picture of the screen, so it is blitted whole, then its palette put on before the
 * letters are drawn over it: the text styles are a ramp matched to the palette that is showing, which is why the
 * glue builds them from `s->palette` rather than from the one on screen. */
static void draw(Screen *s, const GraphicsMenuOps *o)
{
    *s->st->showing = 1;
    o->select(o->ctx, *s->st->screen);
    if (o->plus_bgnd_shape != NULL && o->plus_bgnd_shape(o->ctx, s->pixels, s->palette)) {
        /* port: the screen's palette onto the screen's own, RAW. config_bgnd.c read this palette through
         * fsh_palette_rgb, which shifts the CD's 6-bit VGA values left by one on the way, so it is already
         * display-ready: any other mode shifts it a second time. The destination is the picture's own palette and
         * the source is a separate copy -- palette_put reads and writes the same buffer, so handing it one would
         * rewrite the bytes it is reading, a little further every redraw. */
        memcpy(s->screen_pal, s->palette, sizeof s->screen_pal);
        o->palette_set(o->ctx, s->screen_pal, 0, 0x100, s->palette, FSH_TYPE_PALETTE_8BIT);
        /* port: the palette on the display, once. Options fades the screen out before it opens this one, so without
         * this the page is drawn on black: palette_set only fills the screen's own palette and the one kept for
         * restore, and fade_in is what puts one on the display. */
        if (!s->palette_on) {
            o->fade_in(o->ctx, 0, 0x100, s->palette);
            s->palette_on = 1;
        }
    }
    for (int row = 0; row < GRAPHICS_OWN_SETTINGS; row++) row_text(s, row, o);
    if (o->plus_title != NULL)
        o->plus_title(o->ctx);
    else
        o->plus_text(o->ctx, STYLE_CREAM, TITLE, TITLE_X, TITLE_Y);
    /* port: no letters for DONE and CANCEL. The clean background carries both words in its own corners and the
     * screen used to draw a second copy over them, offset a few pixels -- the capture showed "CANCEL - CANCEL".
     * The rows are still there, still lit and still chosen: only the words are the background's. */
    o->select_default(o->ctx);
    o->pointer_hide(o->ctx);
    o->unlock(o->ctx);
    o->blit_own(o->ctx, (const uint8_t *)(intptr_t)o->picture_shape(o->ctx, *s->st->screen));
    o->unlock(o->ctx);
    o->flip(o->ctx);
    o->pointer_show(o->ctx);
    o->deadline_set(o->ctx, WAIT);
    s->redraw = 0;
}

/* the row a step acts on: the view distance goes through config.dat's own short, the rest through port.cfg */
static void step_row(Screen *s, const GraphicsMenuOps *o, int row, int direction)
{
    PortSettings *const port = s->st->plus->port;
    if (row == GRAPHICS_OWN_VIEW) {
        int16_t view;
        memcpy(&view, s->st->config + GRAPHICS_VIEW, sizeof view);
        graphics_own_view_step(&view, &port->view_ultra);
        view_to(s->st, view);
    } else {
        graphics_own_step(port, row, direction, s->st->plus->fullscreen_format);
    }
    o->sound(o->ctx, CHOSEN);
    s->redraw = 1;
}

/* Space, Enter or the left button */
static void choose(Screen *s, const GraphicsMenuOps *o)
{
    o->pointer_click(o->ctx);
    if (s->armed) {
        o->sound(o->ctx, CHOSEN);
        if (s->selected < GRAPHICS_OWN_SETTINGS)
            step_row(s, o, s->selected, 1);
        else
            s->result = s->selected == GRAPHICS_OWN_DONE;
    }
    o->ticks(o->ctx);
    s->released = s->armed = 0;
    s->rest = 0;
}

/* a key other than the choosing ones */
static void step(Screen *s, uint16_t k, const GraphicsMenuOps *o)
{
    if (k == ESCAPE) {
        s->result = 0;
        o->sound(o->ctx, CHOSEN);
    } else if (k == UP || k == DOWN) {
        o->pointer_off(o->ctx);
        const int from = s->selected;
        const int to = k == DOWN ? (from + 1) % GRAPHICS_OWN_ROWS : (from + GRAPHICS_OWN_ROWS - 1) % GRAPHICS_OWN_ROWS;
        select_row(s, to, o);
        move_to(s->selected, o);
    } else if (k == LEFT || k == RIGHT) {
        if (s->selected < GRAPHICS_OWN_SETTINGS) {
            step_row(s, o, s->selected, k == RIGHT ? 1 : -1);
        } else {
            /* Left and Right walk between Done and Cancel, as the original's last row does */
            o->pointer_off(o->ctx);
            select_row(s, s->selected == GRAPHICS_OWN_DONE ? GRAPHICS_OWN_CANCEL : GRAPHICS_OWN_DONE, o);
            move_to(s->selected, o);
        }
    }
    o->ticks(o->ctx);
    s->armed = 0;
    s->rest = 0;
}

int32_t graphics_own(const GraphicsMenuState *st, const GraphicsMenuOps *ops)
{
    /* not armed: the key that opened the screen may still be down */
    Screen s = { .st = st, .selected = 0, .redraw = 1, .armed = 0, .released = 0, .result = -1, .rest = 0,
                 .bgnd_ready = 0 };
    *st->showing = 1;
    background(&s, ops);
    move_to(s.selected, ops);
    do {
        follow(&s, ops);
        if (s.redraw) draw(&s, ops);
        if (!s.armed) {
            s.rest += ops->ticks(ops->ctx);
            if ((int16_t)s.rest > REST) s.armed = 1;
        }
        if (*st->pointer_active) *st->buttons = (int16_t)ops->pointer_buttons(ops->ctx);
        if (*st->buttons == 0) s.released = 1;
        ops->pointer_activate_if_moved(ops->ctx);
        const uint16_t k = (uint16_t)ops->key(ops->ctx);
        if (k == SPACE || k == ENTER || (*st->buttons == LEFT_BUTTON && s.released))
            choose(&s, ops);
        else if (k == 0)
            s.armed = 1;
        else if (s.armed)
            step(&s, k, ops);
        ops->music(ops->ctx);
        if (*st->net_off == 0) ops->net_sync(ops->ctx);
        /* the deadline, the right button and the quit flag all leave without saving, the way Cancel does */
        if (s.result < 0 && (ops->deadline_passed(ops->ctx) || (*st->buttons == RIGHT_BUTTON && s.released) ||
                             *st->quit == 1)) {
            ops->sound(ops->ctx, CHOSEN);
            s.result = 0;
        }
    } while (s.result < 0);
    *st->showing = 0;
    /* Done saves, the way graphics_menu.c (00423d28) does it once its loop ends; the deadline, the right button and the
     * quit flag all land on Cancel, which saves nothing. */
    if (s.result != 0) {
        if (ops->plus_save != NULL) ops->plus_save(ops->ctx, st->plus->port);
        return 1;
    }
    /* Cancel or Escape: put the settings back as they were on entry, the way graphics_menu.c puts config.dat's
     * image back */
    if (ops->plus_put_back != NULL) ops->plus_put_back(ops->ctx, st->plus->port);
    return 0;
}
