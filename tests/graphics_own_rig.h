/* the fake ops, the rig and the script helpers of test_graphics_own.c and the files split from it, shared (each is its own executable, so
 * the functions are static inline) */
#ifndef TNFS_GRAPHICS_OWN_RIG_H
#define TNFS_GRAPHICS_OWN_RIG_H

#include "game/frontend/graphics_menu.h"
#include "game/frontend/graphics_own.h"
#include "game/race/weather.h"
#include "game/sys/port_profile.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include "core/formats/fsh.h"
#include "game/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* port_settings.c reaches the platform's user files; the test has none */
uint8_t *platform_read_user_file(const char *name, size_t *size) { (void)name, (void)size; return NULL; }
bool platform_write_user_file(const char *name, const uint8_t *data, size_t size)
{
    (void)name, (void)data, (void)size;
    return false;
}
void platform_free_file(uint8_t *data) { (void)data; }

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000,
       PAGE_UP = 0x4900, PAGE_DOWN = 0x5100, LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };

enum { TEXTS = 256, NAV_X0 = 270, NAV_X1 = 370, NAV_Y0 = 432, NAV_Y1 = 476 };

typedef struct Fake {
    int *keys;
    int key_count, key_next;
    int saves, ticks_each;
    int bgnd_calls;          /* how many times the clean background was composed (must be 1: it is cached) */
    int title_draws;         /* the title drawn at the top left */
    char texts[TEXTS][32];   /* every plus_text call, in order */
    int text_count;
    int32_t text_x[TEXTS], text_y[TEXTS];
    int pointer_row;         /* the row the pointer is in (-1: nowhere) */
    int pointer_x, pointer_y; /* where the screen last put the pointer */
    int pointer_in_nav;      /* whether the pointer is in the page indicator */
    char nav_last[16];       /* the page indicator's last text, "1/2 >" or "< 2/2" */
    int hold_at;             /* the pointer stays where hold_x / hold_y put it, as a mouse parked on a row would */
    int32_t hold_x, hold_y;
    int hold_in_nav;         /* the pointer stays on the page indicator, as a mouse parked on it would */
    int pointer_down;        /* the left button, when no scripted sequence is needed */
    const int *button_script; /* buttons read one after the other; the last stays */
    int button_count, button_next;
    int nav_click_at;        /* the button read (1-based) that finds the pointer on the page indicator */
    int panel_calls;
    int bgnd_shapes;         /* times the background was put on the screen (once per draw) */
    int burnt_in_reads;       /* times the original's own screen was asked for its palette */
    int blits;               /* times the page's picture was drawn into the backbuffer */
    int palette_sets;        /* times a palette was put on the screen (the picture's, then the clean background's) */
    int flip_before_bgnd;    /* the screen was presented before the clean background was put on it */
    int flip_before_fade_in; /* the screen was presented before any palette was faded in */
    int fade_ins;            /* times the screen's own palette was put on the display */
    int fade_outs;           /* times the display was faded out on the way out */
    int flips;               /* times the screen was presented, so the fade's place in the run is known */
    int pixels_drawn_before_fade_in;  /* the page's picture was drawn before the palette went on the display */
    int faded_in_before_first_flip;   /* the palette was put on the display before the picture was drawn */
    char events[16];         /* the order of the fades and the flips: I fade in, O fade out, F flip */
    int event_count;
    int bgnd_ok;             /* what plus_bgnd gives: 0 as if config_bgnd_load had failed */
    int dark;                /* what the display says about itself on entry: 1 black, as Options leaves it */
    int put_backs;           /* times the settings were put back (Cancel / Escape only) */
    int aliased_palettes;    /* times palette_set was handed its own destination as the source */
    int non_raw_modes;       /* times palette_set was told to shift an already-shifted palette */
    int dim_draws;            /* times a row the next race reads was drawn dimmed (style 4) */
    int done_draws, cancel_draws;   /* times DONE / CANCEL were drawn over the background's own words */
    int smooth_draws;         /* times the text went through the background-restoring (smooth) op */
    int lit_done, lit_cancel; /* times the lit DONE / CANCEL art was asked for */
    int frees;                /* banks freed */
    int idle_key;             /* what the screen reads once the walk's keys are spent (ESCAPE, as the other walks end) */
    int right_button;         /* the right button reads down once the walk is spent */
    int right_buttons_read;   /* times the buttons have been read since the walk was spent */
    int deadline_passed;      /* the idle deadline reads passed once the walk is spent */
    int16_t *quit;            /* the quit flag the screen reads, so a test can raise it when the walk is spent */
    int quit_when_spent;      /* raise that flag once the walk is spent, not before */
    int band_n;               /* the bands of the pass in progress */
    int band_tops[16], band_bottoms[16];   /* the pass's bands, in the order the screen asked for them */
    int band_best;            /* how many bands the longest pass had */
    int band_best_tops[16], band_best_bottoms[16];   /* the longest pass's bands: every row's own */
    int32_t palette_modes[32];
    int palette_mode_count;
    PortSettings kept;       /* the settings as they were on entry */
} Fake;

static inline int32_t f_key(void *ctx)
{
    Fake *f = ctx;
    if (f->key_next < f->key_count) return f->keys[f->key_next++];
    /* the walk's keys are spent. A quit that was armed is raised here and not before, so the walk runs to its end
     * and the flag is what stops the screen -- the same way the deadline and the button arrive. */
    if (f->quit_when_spent && f->quit != NULL) *f->quit = 1;
    /* Escape ends the screen, as it does for every other walk here. A test that wants a different end sets idle_key,
     * and with it 0 so the screen idles instead and a trigger decides. */
    return f->idle_key;
}
static inline void f_void(void *ctx) { (void)ctx; }
/* the pointer: the screen moves it, and the row it lands in is what pointer_in_rect then works out */
static inline void f_move(void *ctx, int32_t x, int32_t y)
{
    Fake *f = ctx;
    /* the mouse parked on the page indicator: where it is does not change when the screen moves the pointer */
    if (f->hold_in_nav) {
        x = 330;
        y = 452;
    } else if (f->hold_at) {
        x = f->hold_x;
        y = f->hold_y;
    }
    f->pointer_x = x;
    f->pointer_y = y;
    /* Done and Cancel are two words the clean background paints in its bottom corners (config_bgnd.c's CORNERS:
     * 430..475, the left one Done and the right one Cancel). The screen does not draw over them, so the pointer is
     * the only thing that says which of the two the selection has landed on. The page indicator overlaps neither
     * word, but it overlays the menu's broad Done band and therefore must not pretend to be a button. */
    f->pointer_in_nav = NAV_X0 <= x && x < NAV_X1 && NAV_Y0 <= y && y < NAV_Y1;
    if (f->pointer_in_nav)
        f->pointer_row = -1;
    else if (y >= 430 && y < 475)
        f->pointer_row = x < 320 ? GRAPHICS_OWN_DONE : GRAPHICS_OWN_CANCEL;
}
static inline int32_t f_art(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
/* the original's own Graphics screen: its shapes, and the palette 00423d28's open_screen reads as "!pal" -- sixteen
 * bytes of header the way a shape's start, then the colour map. NULL is a bank that has none, which is how the fake
 * says the CD is not there. The lit DONE / CANCEL art is counted as it is asked for. */
static uint8_t art_shape[GRAPHICS_PALETTE];
static const uint8_t *f_shape(void *ctx, int32_t bank, const char *tag)
{
    Fake *f = ctx;
    (void)bank;
    if (strcmp(tag, "Donl") == 0) f->lit_done++;
    if (strcmp(tag, "Canl") == 0) f->lit_cancel++;
    if (strcmp(tag, "!pal") == 0) {
        f->burnt_in_reads++;
        art_shape[0] = 0;
        memset(art_shape + 0x10, 0x20, sizeof art_shape - 0x10);
        return art_shape;
    }
    return NULL;
}
/* Two kinds of palette go onto this screen and they want different modes. The clean background's is already
 * display-ready -- config_bgnd.c read it through fsh_palette_rgb, which shifted the CD's 6-bit values left by one
 * once -- so it must go on RAW or palette_put shifts it a second time, which is what turned the page neon. The
 * original's own bank carries its palette as the exe wrote it, so it is read with the mode its header says. The
 * fake cannot tell them apart by pointer (the screen keeps its own copy in a buffer the fake never sees), so it is
 * told which kind of screen it is looking at. */
static int palette_is_clean;
static inline void f_palette(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    Fake *f = ctx;
    (void)first, (void)count;
    f->palette_sets++;
    /* palette_put writes destination and then copies the result over the screen's copy, so a call whose source
     * IS its destination rewrites the bytes it is reading from: every redraw shifts them a little further. */
    if (src == rgb) f->aliased_palettes++;
    if (palette_is_clean && mode != FSH_TYPE_PALETTE_8BIT) f->non_raw_modes++;
    if (f->palette_mode_count < 32) f->palette_modes[f->palette_mode_count++] = mode;
}
/* The pointer's band: is the pointer where the screen last put it inside this one? That is the whole question, and
 * the pointer's position is all it takes to answer it. */
/* The screen asks about the rows' bands in order, from the first row, on every pass, and stops at the band the
 * pointer is in. So a pass that reaches the last row asked about every row: that pass's bands are the rows' bands.
 * The pass starts again whenever the screen asks about the first row's band (its top is 0). */
static inline int32_t f_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    Fake *f = ctx;
    const int nav = x0 == NAV_X0 && x1 == NAV_X1 && y0 == NAV_Y0 && y1 == NAV_Y1;
    const int nav_half = x0 == NAV_X0 && (x1 == 320 || x1 == NAV_X1) && y0 == NAV_Y0 && y1 == NAV_Y1;
    if (nav || nav_half) return f->pointer_in_nav;
    if (y0 == 0 && f->band_n > 0) {
        if (f->band_n > f->band_best) {
            f->band_best = f->band_n;
            memcpy(f->band_best_tops, f->band_tops, sizeof f->band_tops);
            memcpy(f->band_best_bottoms, f->band_bottoms, sizeof f->band_bottoms);
        }
        f->band_n = 0;
    }
    if (f->band_n < 16) {
        f->band_tops[f->band_n] = y0;
        f->band_bottoms[f->band_n] = y1;
    }
    f->band_n++;
    return x0 <= f->pointer_x && f->pointer_x < x1 && y0 <= f->pointer_y && f->pointer_y < y1;
}
static inline void f_select(void *ctx, int32_t picture) { (void)ctx, (void)picture; }
static inline void f_blit(void *ctx, const uint8_t *s) { (void)s; ((Fake *)ctx)->blits++; }
static inline int32_t f_picture_shape(void *ctx, int32_t p) { (void)ctx, (void)p; return 0; }
static inline void f_select_default(void *ctx) { (void)ctx; }
/* the present: the frame is on the display from here, which is what says whether the palette had already been
 * put there when the previous screen's picture was still up */
static inline void f_flip(void *ctx)
{
    Fake *f = ctx;
    f->flips++;
    f->events[f->event_count < 16 ? f->event_count++ : 15] = 'F';
}
static inline void f_fade(void *ctx, int32_t a, int32_t b)
{
    (void)a, (void)b;
    Fake *f = ctx;
    f->fade_outs++;
    f->events[f->event_count < 16 ? f->event_count++ : 15] = 'O';
}
static inline void f_fade_rgb(void *ctx, int32_t a, int32_t b, const uint8_t *rgb)
{
    (void)a, (void)b, (void)rgb;
    Fake *f = ctx;
    f->fade_ins++;
    /* where it fell in the run: the page must already be drawn when the palette goes on, and the flip that presents
     * them must not have happened yet -- otherwise the frames in between show the previous screen's picture in this
     * screen's palette, and that screen comes out pale for a moment */
    if (f->blits == 0) f->faded_in_before_first_flip = 1;
    if (f->blits > 0) f->pixels_drawn_before_fade_in = 1;
    if (f->flips > 0) f->flip_before_fade_in = 1;
    f->events[f->event_count < 16 ? f->event_count++ : 15] = 'I';
}
static inline void f_deadline(void *ctx, int32_t t) { (void)ctx, (void)t; }
static inline int32_t f_ticks(void *ctx)
{
    const int each = ((const Fake *)ctx)->ticks_each;
    return each != 0 ? each : 100;
}
/* The right button, as a real one arrives: down for the frames after the walk is spent, and held no longer. A press
 * that never comes back up would never let the screen leave, because the menu treats the right button as leaving on a
 * *release* -- the same way the left button chooses -- so it reads buttons==0 once first, which is the release. */
static inline int32_t f_buttons(void *ctx)
{
    Fake *f = ctx;
    if (f->button_script != NULL)
        return f->button_script[f->button_next < f->button_count ? f->button_next++ : f->button_count - 1];
    if (f->pointer_down) return f->pointer_down;
    if (f->key_next < f->key_count) return 0;
    if (!f->right_button) return 0;
    if (f->right_buttons_read == 0) {   /* up: the release that arms the leaving */
        f->right_buttons_read = 1;
        return 0;
    }
    return RIGHT_BUTTON;
}
static inline void f_sound(void *ctx, int32_t sound) { (void)ctx, (void)sound; }
static inline void f_free(void *ctx, int32_t bank) { (void)bank; ((Fake *)ctx)->frees++; }
static inline int32_t f_deadline_passed(void *ctx)
{
    Fake *f = ctx;
    return f->deadline_passed && f->key_next >= f->key_count ? 1 : 0;   /* the walk is spent */
}

/* The row each of the screen's own words belongs to, so the fake can read the selection off the draw: the selected row
 * is drawn lit, and the screen lights exactly one. (Only the settings -- Done and Cancel are the background's
 * words, which f_move reads off the pointer instead.)
 *
 * The two lists are the screen's own: the words in the order it draws them, and the number each of them is. The
 * numbers are the settings' own, so the second page's four are the ones after the first page's eleven, not four more
 * after them. */
static const char *const LABEL[] = { "Perspective Correction", "Texture Filter", "View Distance", "Distance Fog",
                                     "Light Effects", "Field Of View", "Screen Format", "Refresh Rate", "Full Screen",
                                     "Scaling", "Render Threads", "Render Scale" };
static const int LABEL_ROW[] = { GRAPHICS_OWN_PERSPECTIVE, GRAPHICS_OWN_FILTER, GRAPHICS_OWN_VIEW, GRAPHICS_OWN_FOG,
                                 GRAPHICS_OWN_LIGHT, GRAPHICS_OWN_FOV, GRAPHICS_OWN_FORMAT, GRAPHICS_OWN_REFRESH,
                                 GRAPHICS_OWN_FULLSCREEN, GRAPHICS_OWN_SCALING, GRAPHICS_OWN_BANDS, GRAPHICS_OWN_SCALE };
static inline int row_of(const char *text)
{
    for (size_t row = 0; row < sizeof LABEL / sizeof *LABEL; row++)
        if (strcmp(text, LABEL[row]) == 0) return LABEL_ROW[row];
    return -1;
}

static inline void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    Fake *f = ctx;
    if (style == 4) f->dim_draws++;
    /* a lit label names the selection; a lit value can read as a label too ("Fog" is Weather's value as well as a row),
     * so only the label column counts */
    if (style == 1 && x == GRAPHICS_OWN_LABEL_X) {
        const int row = row_of(text);
        if (row >= 0) f->pointer_row = row;
    }
    if (strchr(text, '/') != NULL) snprintf(f->nav_last, sizeof f->nav_last, "%s", text);
    if (strcmp(text, "DONE") == 0) f->done_draws++;
    if (strcmp(text, "CANCEL") == 0) f->cancel_draws++;
    if (f->text_count < TEXTS) {
        snprintf(f->texts[f->text_count], sizeof f->texts[0], "%s", text);
        f->text_x[f->text_count] = x;
        f->text_y[f->text_count] = y;
    }
    f->text_count++;
}
static inline int32_t f_text_width(void *ctx, const char *text)
{
    (void)ctx;
    return (int32_t)strlen(text) * 8;
}
/* the smooth text: the op that puts the background back under the letters before drawing them, which is what the
 * whole page needs, since every redraw repaints all of it */
static inline void f_text_smooth(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    Fake *f = ctx;
    f->smooth_draws++;
    f_text(ctx, style, text, x, y);
}
static inline void f_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    (void)rgb, (void)x0, (void)y0, (void)x1, (void)y1, (void)framed;
    ((Fake *)ctx)->panel_calls++;
}
/* The background is composed into the screen's own buffers, so this op takes no ctx; the fake it is counting on is
 * the one the current rig points at. Counting pins that it is asked for once, not on every redraw. */
static Fake *current;
static inline int32_t f_bgnd(uint8_t pixels[640 * 480], uint8_t palette[768])
{
    (void)pixels, (void)palette;
    current->bgnd_calls++;
    return current->bgnd_ok;
}
/* the title: drawn at the top left, over the clean background */
static inline void f_title(void *ctx, const char *text, int32_t x, int32_t y)
{
    (void)text, (void)x, (void)y;
    ((Fake *)ctx)->title_draws++;
}
/* the clean background put on the screen with its palette. A screen whose background it was cannot compose has no
 * shape to put up: the fake says so the way the glue does when own_shape is NULL, so the run shows what is really
 * on the display. */
static inline int32_t f_bgnd_shape(void *ctx, const uint8_t pixels[640 * 480], const uint8_t palette[768])
{
    (void)pixels, (void)palette;
    Fake *f = ctx;
    f->bgnd_shapes++;
    if (f->flips == 0) f->flip_before_bgnd = 1;   /* the clean background went down after a present: the old
                                                    * picture was shown first */
    return f->bgnd_ok;
}
/* Cancel / Escape: what puts the settings back */
static inline void f_put_back(void *ctx, PortSettings *settings)
{
    ((Fake *)ctx)->put_backs++;
    *settings = ((Fake *)ctx)->kept;
}
static inline void f_save(void *ctx, const PortSettings *settings)
{
    (void)settings;
    ((Fake *)ctx)->saves++;
}
/* whether the display is already dark, as Options leaves it after fading the display out before opening this screen.
 * The fake's `dark` says so; 0 is a display with a menu on it. */
static inline int32_t f_display_dark(void *ctx)
{
    return ((Fake *)ctx)->dark;
}

/* named slots, so the four port slots at the end cannot be confused with the original's thirty-six */
static GraphicsMenuOps OPS = {
    .pointer_move_to = f_move, .load_art = f_art, .load_inverse = f_art, .music_feed = f_void, .shape = f_shape,
    .palette_set = f_palette, .pointer_poll = f_void, .pointer_mark = f_void, .pointer_in_rect = f_in_rect,
    .select = f_select, .blit_own = f_blit, .picture_shape = f_picture_shape, .select_default = f_select_default,
    .pointer_hide = f_void, .unlock = f_void, .flip = f_flip, .pointer_show = f_void, .pointer_buttons = f_buttons,
    .text_styles = f_void, .fade_in = f_fade_rgb, .deadline_set = f_deadline, .sound = f_sound, .lock = f_void,
    .ticks = f_ticks, .pointer_activate_if_moved = f_void, .key = f_key, .pointer_click = f_void,
    .pointer_off = f_void, .music = f_void, .net_sync = f_void, .deadline_passed = f_deadline_passed,
    .fade_out = f_fade, .free_inverse = f_void, .free = f_free,
    /* port: the rows' text, the values' width, the panel, and Done's save */
    .plus_text = f_text, .plus_text_width = f_text_width, .plus_panel = f_panel, .plus_save = f_save,
    /* port: the clean background composed once (cached) and put on the screen, then the title, then Cancel's
     * put-back */
    .plus_bgnd = f_bgnd, .plus_bgnd_shape = f_bgnd_shape, .plus_title = f_title, .plus_put_back = f_put_back,
    .plus_text_smooth = f_text_smooth,
    .plus_display_dark = f_display_dark,
};

typedef struct Rig {
    Fake f;
    GraphicsMenuState st;
    PortSettings port;
    uint8_t config[GRAPHICS_KEPT];
    int16_t showing, net_off, pointer_active, buttons, quit;
    int32_t screen;
    int32_t inverse;
    GraphicsPlus plus;
} Rig;

static inline void rig_init(Rig *r)
{
    memset(r, 0, sizeof *r);
    r->f.pointer_row = -1;
    r->port = port_settings_defaults();
    r->port.profile = PROFILE_RESTOMOD;   /* the mode that leaves every row free */
    r->f.kept = r->port;
    r->net_off = 1;   /* the Options art, not check/graphics.fsh */
    r->st.config = r->config;
    r->st.showing = &r->showing;
    r->st.net_off = &r->net_off;
    r->st.pointer_active = &r->pointer_active;
    r->st.buttons = &r->buttons;
    r->st.screen = &r->screen;
    r->st.inverse = &r->inverse;
    r->st.quit = &r->quit;
    r->f.idle_key = ESCAPE;  /* the walk's keys spent: Escape ends the screen, as the other walks end */
    r->f.right_button = r->f.deadline_passed = 0;
    r->f.bgnd_ok = 1;        /* the CD screens compose into a clean background */
    r->f.dark = 0;           /* port: a display with something on it, so the fade on the way in has work to do. The
                              * walk from Options is the other case and says so itself (see below). */
    r->f.quit = &r->quit;    /* the screen reads the flag the fake raises */
    r->st.plus = &r->plus;
    r->plus.port = &r->port;
    r->plus.fullscreen_format = SCREEN_21_9;
    OPS.ctx = &r->f;
    current = &r->f;   /* the ops are one table shared by the tests; each rig points it at its own fake */
}

/* whether `text` was drawn at all */
static inline int drew(const Fake *f, const char *text)
{
    for (int i = 0; i < f->text_count && i < TEXTS; i++)
        if (strcmp(f->texts[i], text) == 0) return 1;
    return 0;
}


/* The key script the screen reads: one key per iteration, and it arms on the first iteration's own ticks, so the
 * whole walk belongs in a single session -- re-entering the screen starts again on the first row. */
enum { SCRIPT_MAX = 64 };
static int script[SCRIPT_MAX];

/* Down to `row` from the first, then `key` on it `times` times (each step needs an idle iteration between it and the
 * next, or the menu reads both in one go), then how the screen ends. `end` is DONE, which keeps the settings, or
 * ESCAPE, which puts them back as they were on entry -- so a test that reads a setting afterwards has to leave by
 * Done. Returns how many keys that is. */
static inline int script_walk(int row, int key, int times, int end)
{
    int n = 0;
    for (int i = 0; i < row; i++) script[n++] = DOWN;
    for (int i = 0; i < times; i++) {
        script[n++] = key;
        if (i + 1 < times) script[n++] = 0;
    }
    if (end == GRAPHICS_OWN_DONE) {
        for (int i = row; i < GRAPHICS_OWN_DONE; i++) script[n++] = DOWN;
        script[n++] = 0;      /* the walk's last key left it unarmed; the choice needs an idle iteration first */
        script[n++] = SPACE;
        script[n++] = 0;
    }
    script[n++] = ESCAPE;
    return n;
}

/* the walk down to a row and one RIGHT on it, then nothing: no Done, no Cancel, no Escape. What ends the screen is
 * whatever the test armed -- the idle deadline, the right button or the quit flag -- so the setting is changed and the
 * exit is not a decision. `count` receives the number of keys. */
static inline void change_a_setting_then_settle(int row, int key, int *count)
{
    int n = 0;
    for (int i = 0; i < row; i++) script[n++] = DOWN;
    script[n++] = key;
    script[n++] = 0;      /* the step's own iteration leaves it unarmed, so the screen idles and settles */
    *count = n;
}

/* Runs the screen once with the script, and returns what it returned (1 for Done, 0 for Cancel). */
static inline int32_t run_script(Rig *r, int count)
{
    r->f.keys = script;
    r->f.key_count = count;
    r->f.key_next = 0;
    palette_is_clean = r->f.bgnd_ok;   /* what the raw-mode rule applies to: the clean background's own palette */
    return graphics_own(&r->st, &OPS);
}

/* the same, but with nothing to press once the walk is spent: the screen idles, so only a trigger -- the deadline,
 * the right button, the quit flag -- can end it. That is the case 00423d28 treats as leaving without deciding. */
static inline int32_t run_script_idling(Rig *r, int count)
{
    r->f.idle_key = 0;
    return run_script(r, count);
}

/* the same, with the quit flag armed to go up once the walk is spent: the screen reads it through st.quit, so the flag
 * is raised at the moment the walk ends rather than before it -- otherwise the flag would stop the screen on its very
 * first iteration and the setting would never have been changed. */
static inline int32_t run_script_quitting(Rig *r, int count)
{
    r->f.quit_when_spent = 1;
    return run_script_idling(r, count);
}
#endif
