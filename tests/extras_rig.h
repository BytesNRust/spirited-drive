/* the fake ops, the rig and the script helpers of test_extras_menu.c and the files split from it, shared (each is its own executable, so
 * the functions are static inline) */
#ifndef TNFS_EXTRAS_RIG_H
#define TNFS_EXTRAS_RIG_H

#include "game/frontend/extras_menu.h"
#include "game/sys/port_extras.h"
#include "game/sys/port_profile.h"
#include "test.h"

#include "game/platform/platform.h"

#include <string.h>

/* port_extras_load/save reads / writes user/extras.cfg through the platform; the test has no file */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       PAGE_DOWN = 0x5100 };
/* the game's text styles (text.h): 0 cream, 1 yellow, 2 red, 3 white */
enum { STYLE_CREAM = 0, STYLE_YELLOW = 1, STYLE_RED = 2, STYLE_DIM = 4 };

typedef struct Fake {
    const int *keys;
    int key_count, key_next;
    int saves;
    int log_opens;   /* the Open the log folder row's action, run */
    PortExtras saved;
    int16_t saved_unlock;
    char drawn[512][40];
    int drawn_count;
    int32_t style_of[512];   /* the style each word was drawn in */
    int32_t y_of[512];       /* and where, so a column can be checked */
    int smooth_draws;        /* words drawn through the background-restoring (smooth) op */
    int fade_ins;            /* times the screen's own palette was put on the display */
    int fade_outs;           /* times the display was faded out on the way out */
    int blits;               /* times a picture was drawn into the backbuffer */
    int flips;               /* times the page was presented, so the fade's place in the run is known */
    int pixels_drawn_before_fade_in;  /* the page was drawn before the palette went on the display */
    int style_rebuilds;      /* times the styles were rebuilt for the screen's palette */
    int styles_faded;        /* the rebuilds that came after the palette had already gone on */
    char events[16];         /* the order of the fades and the flips: I fade in, O fade out, F flip */
    int event_count;
    int pointer_in_nav;
    int bgnd_asks;   /* how many times the screen asked for the clean background */
    int panel_after_bgnd;   /* the background was put on the screen */
    int panel_before_bgnd;  /* the dark panel was drawn before it, over the caller's picture */
    int bgnd_ok;          /* what config_bgnd_load gives: 0 as if the CD had none */
    int panels;           /* dark panels drawn */
    int titles;           /* titles drawn through the title op */
    int headers;          /* headers drawn through the header op */
    int32_t header_width; /* the last one's underline */
    int resets;           /* Restore's actions run */
} Fake;

static inline int32_t f_key(void *ctx)
{
    Fake *f = ctx;
    return f->key_next < f->key_count ? f->keys[f->key_next++] : ESCAPE;
}
static inline void f_styles(void *ctx)
{
    Fake *f = ctx;
    f->style_rebuilds++;
    /* the styles are a ramp matched to the palette on the display, so a rebuild that comes before the screen's own
     * palette has gone on rebuilds them from the caller's -- which is a different menu's menu */
    if (f->fade_ins > 0) f->styles_faded++;
}
static inline int32_t f_ticks(void *ctx) { (void)ctx; return 100; }
static inline int32_t f_zero(void *ctx) { (void)ctx; return 0; }
static inline void f_void(void *ctx) { (void)ctx; }
static inline void f_one(void *ctx, int32_t v) { (void)ctx, (void)v; }
static inline void f_move(void *ctx, int32_t x, int32_t y) { (void)ctx, (void)x, (void)y; }
static inline int32_t f_art(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
static const uint8_t *f_shape(void *ctx, int32_t bank, const char *tag)
{
    static uint8_t shape[16];
    (void)ctx, (void)bank, (void)tag;
    return shape;
}
static inline void f_palette(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx, (void)rgb, (void)first, (void)count, (void)src, (void)mode;
}
static inline int32_t f_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    Fake *f = ctx;
    return f->pointer_in_nav && x0 == EXTRAS_NAV_X0 && x1 == EXTRAS_NAV_X1 && y0 == EXTRAS_NAV_Y0 && y1 == EXTRAS_NAV_Y1;
}
/* The screen starts a redraw by putting the caller's picture up, so that is where the log of words begins again:
 * the rig keeps the last frame the screen drew, which is the page as it stands, not every page it passed through. */
static inline void f_select(void *ctx, int32_t picture)
{
    Fake *f = ctx;
    (void)picture;
    f->drawn_count = 0;
    f->smooth_draws = 0;
}
/* the clean background, asked for once as the screen opens (config_bgnd_load takes about half a second) */
static inline int32_t f_bgnd(void *ctx, uint8_t *pixels, uint8_t *palette)
{
    Fake *f = ctx;
    f->bgnd_asks++;
    memset(pixels, 0x2a, EXTRAS_BGND_W * EXTRAS_BGND_H);
    memset(palette, 0x10, 768);
    return f->bgnd_ok;
}
static inline int32_t f_bgnd_shape(void *ctx, const uint8_t *pixels, const uint8_t *palette)
{
    Fake *f = ctx;
    (void)pixels, (void)palette;
    f->panel_after_bgnd = 1;   /* the background is down; whatever is drawn next lands on it */
    return 1;
}
static inline void f_blit(void *ctx, const uint8_t *shape)
{
    (void)shape;
    ((Fake *)ctx)->blits++;
}
static inline int32_t f_picture(void *ctx, int32_t picture) { (void)ctx; return picture; }
/* the present: from here the frame is on the display, which is what says whether the palette was already on it */
static inline void f_flip(void *ctx)
{
    Fake *f = ctx;
    f->flips++;
    f->events[f->event_count < 16 ? f->event_count++ : 15] = 'F';
}
static inline void f_fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)first, (void)count, (void)rgb;
    Fake *f = ctx;
    f->fade_ins++;
    /* where it fell in the run: the page must already be drawn when the palette goes on */
    if (f->blits > 0) f->pixels_drawn_before_fade_in = 1;
    f->events[f->event_count < 16 ? f->event_count++ : 15] = 'I';
}
static inline void f_fade_out(void *ctx, int32_t first, int32_t count)
{
    (void)first, (void)count;
    Fake *f = ctx;
    f->fade_outs++;
    f->events[f->event_count < 16 ? f->event_count++ : 15] = 'O';
}
static inline int32_t f_buttons(void *ctx) { (void)ctx; return 0; }
static inline void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    Fake *f = ctx;
    (void)x;
    if (f->drawn_count < 512) {
        snprintf(f->drawn[f->drawn_count], sizeof f->drawn[0], "%s", text);
        f->style_of[f->drawn_count] = style;
        f->y_of[f->drawn_count] = y;
        f->drawn_count++;
    }
}
/* the smooth draw: the op that puts the clean background back under the letters before writing them, which the whole
 * page needs since every redraw repaints all of it */
static inline void f_text_smooth(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    ((Fake *)ctx)->smooth_draws++;
    f_text(ctx, style, text, x, y);
}
static inline int32_t f_text_width(void *ctx, const char *text) { (void)ctx; return (int32_t)strlen(text) * 10; }
static inline void f_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    Fake *f = ctx;
    (void)rgb, (void)x0, (void)y0, (void)x1, (void)y1, (void)framed;
    if (f->bgnd_asks == 0) f->panel_before_bgnd++;
    f->panels++;
}
/* the header op: recorded with the words, as the red they replace */
static inline void f_header(void *ctx, const char *text, int32_t x, int32_t y, int32_t width)
{
    Fake *f = ctx;
    f->headers++;
    f->header_width = width;
    f_text(ctx, 2, text, x, y);
}
/* the title op: recorded with the words, in red */
static inline void f_title(void *ctx, const char *text, int32_t x, int32_t y)
{
    ((Fake *)ctx)->titles++;
    f_text(ctx, 2, text, x, y);
}
static inline void f_open_log(void *ctx)
{
    Fake *f = ctx;
    f->log_opens++;
}
static inline void f_save(void *ctx, const PortExtras *e, int16_t unlock)
{
    Fake *f = ctx;
    f->saves++;
    f->saved = *e;
    f->saved_unlock = unlock;
}

static inline int shown(const Fake *f, const char *text)
{
    for (int i = 0; i < f->drawn_count; i++)
        if (strcmp(f->drawn[i], text) == 0) return 1;
    return 0;
}

/* the style `text` was drawn in, -1 if it was never drawn */
static inline int32_t style_in(const Fake *f, const char *text)
{
    for (int i = 0; i < f->drawn_count; i++)
        if (strcmp(f->drawn[i], text) == 0) return f->style_of[i];
    return -1;
}

/* the y `text` was drawn at, -1 if it was never drawn */
static inline int32_t y_in(const Fake *f, const char *text)
{
    for (int i = 0; i < f->drawn_count; i++)
        if (strcmp(f->drawn[i], text) == 0) return f->y_of[i];
    return -1;
}

/* one line of the front-end font, graver18 at 1/1, rounded up generously: the page has to fit between the title and
 * the DONE/CANCEL band, and the text's own height is what decides whether it does */
enum { LINE = 20 };

/* the pitch the rows are set at (extras_menu.c's ROW_PITCH): the spacing between two rows of a group */
enum { PITCH = 20 };

static inline void f_reset(void *ctx, PortExtras *e)
{
    Fake *f = ctx;
    f->resets++;
    e->units = UNITS_MPH;   /* what the mode's values have */
}

typedef struct Rig {
    Fake fake;
    PortExtras e;
    int16_t unlock;
    PortExtras saved;
    int16_t saved_unlock;
    int profile;
} Rig;

static inline void setup(Rig *r)
{
    memset(r, 0, sizeof *r);
    r->profile = PROFILE_RESTOMOD;   /* the mode that leaves every row free */
    r->e = port_extras_defaults();
    r->unlock = 0;
    r->fake.bgnd_ok = 1;   /* the CD has the configuration screens */
}

static inline void run_with(Rig *r, const int *keys, int count, void (*smooth)(void *, int32_t, const char *, int32_t,
                                                                        int32_t))
{
    r->fake.keys = keys;
    r->fake.key_count = count;
    const ExtrasMenuOps ops = {
        .pointer_move_to = f_move, .load_art = f_art, .shape = f_shape, .palette_set = f_palette,
        .pointer_in_rect = f_in_rect, .select = f_select, .blit_own = f_blit, .picture_shape = f_picture,
        .select_default = f_void, .pointer_hide = f_void, .flip = f_flip, .pointer_show = f_void,
        .pointer_buttons = f_buttons, .text_styles = f_styles, .fade_in = f_fade_in, .deadline_set = f_one,
        .fade_out = f_fade_out,
        .deadline_passed = f_zero, .sound = f_one, .ticks = f_ticks, .pointer_activate_if_moved = f_void,
        .key = f_key, .pointer_click = f_void, .pointer_off = f_void, .music = f_void,
        .plus_text = f_text, .plus_text_width = f_text_width, .plus_panel = f_panel,
        .plus_bgnd = f_bgnd, .plus_bgnd_shape = f_bgnd_shape,
        .plus_text_smooth = smooth, .plus_title = f_title, .plus_header = f_header,
        .extras_save = f_save, .ctx = &r->fake, .open_log_folder = f_open_log, .profile = r->profile, .reset_profile = f_reset };
    (void)extras_menu_run(&r->e, &r->unlock, 1, &ops);
}

static inline void run(Rig *r, const int *keys, int count) { run_with(r, keys, count, f_text_smooth); }

/* DOWN `count` times, appended to keys from n on; returns the new count */
static inline int downs(int *keys, int n, int count)
{
    for (int i = 0; i < count; i++) keys[n++] = DOWN;
    return n;
}

/* the flat list of rows splits into the three pages at ROW_FIRST_P2 and ROW_FIRST_P3 */
static inline int page_of(int row) { return row < ROW_FIRST_P2 ? 0 : row < ROW_FIRST_P3 ? 1 : 2; }
static inline int first_on(int page) { return page == 0 ? 0 : page == 1 ? ROW_FIRST_P2 : ROW_FIRST_P3; }
/* a row's index on its own page */
static inline int index_of(int row) { return row - first_on(page_of(row)); }
/* how many rows its page shows */
static inline int rows_of_page(int page) { return page == 0 ? ROWS_P1 : page == 1 ? ROWS_P2 : ROWS_P3; }
static inline int rows_on(int row) { return rows_of_page(page_of(row)); }

/* the DOWNs from a row to its page's Done, which is the row past the page's last switch */
static inline int downs_to_done(int row) { return rows_on(row) - index_of(row); }

/* the keys from the screen's first row to `row`: for each page before its own, DOWN to Done, RIGHT to Cancel, RIGHT round to the next page's first row; then DOWN to
 * the row. Returns how many keys were written. */
static inline int keys_to_row(int *keys, int row)
{
    int n = 0;
    for (int page = 0; page < page_of(row); page++) {
        n = downs(keys, n, rows_of_page(page));
        keys[n++] = RIGHT, keys[n++] = RIGHT;
    }
    return downs(keys, n, index_of(row));
}
#endif
