/* The port's own Graphics screen (graphics_own.c): the nine rows in the owner's order over the clean configuration
 * background, its own title, and Done/Cancel as Options has them. Driven through the fake ops with a key script, the
 * same rig test_graphics_plus.c uses.
 *
 * What is pinned here:
 *   - the row order and the exact labels, "Perspective Correction" not "Modern 3D";
 *   - every row's value text, including the view distance's fourth value "Ultra";
 *   - that each row writes into the settings the owner named (scale_integer, view_ultra, fov_adjust, format,
 *     refresh_hz, fullscreen, and the three that moved in from EXTRAS);
 *   - Done saving, Cancel putting back, Escape putting back;
 *   - the clean background being asked for exactly once and then cached, and the title drawn at the top left. */
#include "game/frontend/graphics_menu.h"
#include "game/frontend/graphics_own.h"
#include "game/sys/port_settings.h"
#include "test.h"

#include "core/formats/fsh.h"
#include "game/platform/platform.h"

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
       LEFT_BUTTON = 1, RIGHT_BUTTON = 2 };

typedef struct Fake {
    int *keys;
    int key_count, key_next;
    int saves, ticks_each;
    int bgnd_calls;          /* how many times the clean background was composed (must be 1: it is cached) */
    int title_draws;         /* the title drawn at the top left */
    char texts[96][28];      /* every plus_text call, in order */
    int text_count;
    int32_t text_x[96], text_y[96];
    int pointer_row;         /* the row the pointer is in (-1: nowhere) */
    int pointer_x, pointer_y; /* where the screen last put the pointer */
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
    int32_t palette_modes[32];
    int palette_mode_count;
    PortSettings kept;       /* the settings as they were on entry */
} Fake;

static int32_t f_key(void *ctx)
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
static void f_void(void *ctx) { (void)ctx; }
/* the pointer: the screen moves it, and the row it lands in is what pointer_in_rect then works out */
static void f_move(void *ctx, int32_t x, int32_t y)
{
    Fake *f = ctx;
    f->pointer_x = x;
    f->pointer_y = y;
    /* Done and Cancel are two words the clean background paints in its bottom corners (config_bgnd.c's CORNERS:
     * 430..475, the left one Done and the right one Cancel). The screen does not draw over them, so the pointer is
     * the only thing that says which of the two the selection has landed on. */
    if (y >= 430 && y < 475) f->pointer_row = x < 320 ? GRAPHICS_OWN_DONE : GRAPHICS_OWN_CANCEL;
}
static int32_t f_art(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
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
static void f_palette(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
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
static int32_t f_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    const Fake *f = ctx;
    return x0 <= f->pointer_x && f->pointer_x < x1 && y0 <= f->pointer_y && f->pointer_y < y1;
}
static void f_select(void *ctx, int32_t picture) { (void)ctx, (void)picture; }
static void f_blit(void *ctx, const uint8_t *s) { (void)s; ((Fake *)ctx)->blits++; }
static int32_t f_picture_shape(void *ctx, int32_t p) { (void)ctx, (void)p; return 0; }
static void f_select_default(void *ctx) { (void)ctx; }
/* the present: the frame is on the display from here, which is what says whether the palette had already been
 * put there when the previous screen's picture was still up */
static void f_flip(void *ctx)
{
    Fake *f = ctx;
    f->flips++;
    f->events[f->event_count < 16 ? f->event_count++ : 15] = 'F';
}
static void f_fade(void *ctx, int32_t a, int32_t b)
{
    (void)a, (void)b;
    Fake *f = ctx;
    f->fade_outs++;
    f->events[f->event_count < 16 ? f->event_count++ : 15] = 'O';
}
static void f_fade_rgb(void *ctx, int32_t a, int32_t b, const uint8_t *rgb)
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
static void f_deadline(void *ctx, int32_t t) { (void)ctx, (void)t; }
static int32_t f_ticks(void *ctx)
{
    const int each = ((const Fake *)ctx)->ticks_each;
    return each != 0 ? each : 100;
}
/* The right button, as a real one arrives: down for the frames after the walk is spent, and held no longer. A press
 * that never comes back up would never let the screen leave, because the menu treats the right button as leaving on a
 * *release* -- the same way the left button chooses -- so it reads buttons==0 once first, which is the release. */
static int32_t f_buttons(void *ctx)
{
    Fake *f = ctx;
    if (f->key_next < f->key_count) return 0;
    if (!f->right_button) return 0;
    if (f->right_buttons_read == 0) {   /* up: the release that arms the leaving */
        f->right_buttons_read = 1;
        return 0;
    }
    return RIGHT_BUTTON;
}
static void f_sound(void *ctx, int32_t sound) { (void)ctx, (void)sound; }
static void f_free(void *ctx, int32_t bank) { (void)bank; ((Fake *)ctx)->frees++; }
static int32_t f_deadline_passed(void *ctx)
{
    Fake *f = ctx;
    return f->deadline_passed && f->key_next >= f->key_count ? 1 : 0;   /* the walk is spent */
}

/* The row each of the screen's own words belongs to, so the fake can read the selection off the draw: the selected row
 * is drawn lit, and the screen lights exactly one. (Only the nine settings -- Done and Cancel are the background's
 * words, which f_move reads off the pointer instead.) */
static int row_of(const char *text)
{
    static const char *const LABEL[GRAPHICS_OWN_SETTINGS] = { "Perspective Correction", "Texture Filter", "Scaling",
                                                             "View Distance", "Fog", "Field of view", "Screen format",
                                                             "Refresh rate", "Full screen" };
    for (int row = 0; row < GRAPHICS_OWN_SETTINGS; row++)
        if (strcmp(text, LABEL[row]) == 0) return row;
    return -1;
}

static void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    Fake *f = ctx;
    if (style == 4) f->dim_draws++;
    if (style == 1) {
        const int row = row_of(text);
        if (row >= 0) f->pointer_row = row;
    }
    if (strcmp(text, "DONE") == 0) f->done_draws++;
    if (strcmp(text, "CANCEL") == 0) f->cancel_draws++;
    if (f->text_count < 96) {
        snprintf(f->texts[f->text_count], sizeof f->texts[0], "%s", text);
        f->text_x[f->text_count] = x;
        f->text_y[f->text_count] = y;
    }
    f->text_count++;
}
static int32_t f_text_width(void *ctx, const char *text)
{
    (void)ctx;
    return (int32_t)strlen(text) * 8;
}
/* the smooth text: the op that puts the background back under the letters before drawing them, which is what the
 * whole page needs, since every redraw repaints all of it */
static void f_text_smooth(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    Fake *f = ctx;
    f->smooth_draws++;
    f_text(ctx, style, text, x, y);
}
static void f_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    (void)rgb, (void)x0, (void)y0, (void)x1, (void)y1, (void)framed;
    ((Fake *)ctx)->panel_calls++;
}
/* The background is composed into the screen's own buffers, so this op takes no ctx; the fake it is counting on is
 * the one the current rig points at. Counting pins that it is asked for once, not on every redraw. */
static Fake *current;
static int32_t f_bgnd(uint8_t pixels[640 * 480], uint8_t palette[768])
{
    (void)pixels, (void)palette;
    current->bgnd_calls++;
    return current->bgnd_ok;
}
/* the title: drawn at the top left, over the clean background */
static void f_title(void *ctx, const char *text, int32_t x, int32_t y)
{
    (void)text, (void)x, (void)y;
    ((Fake *)ctx)->title_draws++;
}
/* the clean background put on the screen with its palette. A screen whose background it was cannot compose has no
 * shape to put up: the fake says so the way the glue does when own_shape is NULL, so the run shows what is really
 * on the display. */
static int32_t f_bgnd_shape(void *ctx, const uint8_t pixels[640 * 480], const uint8_t palette[768])
{
    (void)pixels, (void)palette;
    Fake *f = ctx;
    f->bgnd_shapes++;
    if (f->flips == 0) f->flip_before_bgnd = 1;   /* the clean background went down after a present: the old
                                                    * picture was shown first */
    return f->bgnd_ok;
}
/* Cancel / Escape: what puts the settings back */
static void f_put_back(void *ctx, PortSettings *settings)
{
    ((Fake *)ctx)->put_backs++;
    *settings = ((Fake *)ctx)->kept;
}
static void f_save(void *ctx, const PortSettings *settings)
{
    (void)settings;
    ((Fake *)ctx)->saves++;
}
/* whether the display is already dark, as Options leaves it after fading the display out before opening this screen.
 * The fake's `dark` says so; 0 is a display with a menu on it. */
static int32_t f_display_dark(void *ctx)
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

static void rig_init(Rig *r)
{
    memset(r, 0, sizeof *r);
    r->f.pointer_row = -1;
    r->port = port_settings_defaults();
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
static int drew(const Fake *f, const char *text)
{
    for (int i = 0; i < f->text_count && i < 96; i++)
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
static int script_walk(int row, int key, int times, int end)
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
static void change_a_setting_then_settle(int row, int key, int *count)
{
    int n = 0;
    for (int i = 0; i < row; i++) script[n++] = DOWN;
    script[n++] = key;
    script[n++] = 0;      /* the step's own iteration leaves it unarmed, so the screen idles and settles */
    *count = n;
}

/* Runs the screen once with the script, and returns what it returned (1 for Done, 0 for Cancel). */
static int32_t run_script(Rig *r, int count)
{
    r->f.keys = script;
    r->f.key_count = count;
    r->f.key_next = 0;
    palette_is_clean = r->f.bgnd_ok;   /* what the raw-mode rule applies to: the clean background's own palette */
    return graphics_own(&r->st, &OPS);
}

/* the same, but with nothing to press once the walk is spent: the screen idles, so only a trigger -- the deadline,
 * the right button, the quit flag -- can end it. That is the case 00423d28 treats as leaving without deciding. */
static int32_t run_script_idling(Rig *r, int count)
{
    r->f.idle_key = 0;
    return run_script(r, count);
}

/* the same, with the quit flag armed to go up once the walk is spent: the screen reads it through st.quit, so the flag
 * is raised at the moment the walk ends rather than before it -- otherwise the flag would stop the screen on its very
 * first iteration and the setting would never have been changed. */
static int32_t run_script_quitting(Rig *r, int count)
{
    r->f.quit_when_spent = 1;
    return run_script_idling(r, count);
}

/* 1: the nine labels, in the owner's order, and nothing called "Modern 3D" */
static void test_rows_in_order(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);

    static const char *const want[] = { "Perspective Correction", "Texture Filter", "Scaling", "View Distance",
                                        "Fog", "Field of view", "Screen format", "Refresh rate", "Full screen" };
    CHECK_EQ_INT(GRAPHICS_OWN_SETTINGS, 9);
    for (int row = 0; row < GRAPHICS_OWN_SETTINGS; row++) {
        CHECK(drew(&r.f, want[row]));
        /* each label drawn at the same left edge: the labels are a column */
        for (int i = 0; i < r.f.text_count && i < 96; i++)
            if (strcmp(r.f.texts[i], want[row]) == 0) CHECK_EQ_INT(r.f.text_x[i], GRAPHICS_OWN_LABEL_X);
    }
    CHECK(!drew(&r.f, "Modern 3D"));
}

/* 2: the title at the top left, drawn once per draw */
static void test_title_top_left(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    CHECK(r.f.title_draws > 0);
    CHECK_EQ_INT(r.f.bgnd_calls, 1);   /* composed once and cached, not per redraw */
}

/* 3: the screen's own palette reaches the display. Options fades the screen out before it opens this one, and only
 * fade_in puts a palette on the display, so without it the whole page is drawn on black. */
static void test_palette_reaches_the_display(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    CHECK(r.f.fade_ins > 0);
}

/* 4: palette_set is never handed its own destination as the source. palette_put reads src and writes the same
 * buffer, so a call that aliases shifts every colour byte left by two ONCE PER REDRAW; after a few redraws the
 * bytes have overflowed and the page is neon. This is what the capture showed. */
static void test_palette_is_not_rewritten_in_place(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    CHECK(r.f.aliased_palettes == 0);
}

/* 5: the palette goes on raw. The clean background's palette is already display-ready -- config_bgnd.c reads it
 * through fsh_palette_rgb, which shifts the CD's 6-bit VGA values left by one once. Passing any other mode makes
 * palette_put shift them a second time, which is the other half of the neon. */
static void test_palette_is_put_raw(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    CHECK(r.f.palette_mode_count > 0);
    CHECK(r.f.non_raw_modes == 0);
}

/* 6: DONE and CANCEL are not drawn over the background's own. The clean configuration background carries those two
 * words in its corners, so the screen drawing its own lands a second, slightly offset copy beside the first -- the
 * headless capture showed "CANCEL - CANCEL". The rows stay selectable; only the letters are the background's. */
static void test_done_and_cancel_are_not_drawn_twice(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    CHECK_EQ_INT(r.f.done_draws, 0);
    CHECK_EQ_INT(r.f.cancel_draws, 0);
}

/* port: DONE and CANCEL light up when chosen, as Options' own art (Donl / Canl) over the background's words, and
 * stay dark otherwise. Both banks are freed as the screen closes: Options' own, for that art, and the original's
 * burnt-in Graphics screen. */
static void test_done_and_cancel_light_up(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    CHECK_EQ_INT(r.f.lit_done, 0);
    CHECK_EQ_INT(r.f.lit_cancel, 0);
    CHECK_EQ_INT(r.f.frees, 2);
    Rig done;
    rig_init(&done);
    run_script(&done, script_walk(GRAPHICS_OWN_VIEW, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK(done.f.lit_done > 0);
    CHECK_EQ_INT(done.f.lit_cancel, 0);
    Rig cancel;
    rig_init(&cancel);
    int n = 0;
    for (int i = 0; i < GRAPHICS_OWN_CANCEL; i++) script[n++] = DOWN;
    script[n++] = ESCAPE;
    run_script(&cancel, n);
    CHECK(cancel.f.lit_cancel > 0);
}

/* 7: the text goes through the background-restoring op. Every redraw repaints the whole page -- the screen blits
 * the clean background whole and then writes the letters over it -- so the letters have to land through the draw
 * that puts the background back underneath first, the way Options' own rows do. Without it the letters are written
 * straight onto whatever was on the picture and a move leaves the old row's edges behind. */
static void test_text_is_drawn_over_the_background(void)
{
    Rig r;
    rig_init(&r);
    script[0] = ESCAPE;
    run_script(&r, 1);
    /* every word on the page, both times over: the screen must not fall back to the plain draw for any of them */
    CHECK(r.f.smooth_draws > 0);
    CHECK_EQ_INT(r.f.smooth_draws, r.f.text_count);
}

/* 8: the smooth draw is used only where there is a background to restore. The original screen (the race's pause
 * menu) has the burnt-in picture rather than the clean one, so the glue leaves this slot empty and the rows must
 * still be drawn. */
static void test_rows_are_drawn_without_a_smooth_op(void)
{
    Rig r;
    rig_init(&r);
    OPS.plus_text_smooth = NULL;
    script[0] = ESCAPE;
    run_script(&r, 1);
    OPS.plus_text_smooth = f_text_smooth;
    CHECK_EQ_INT(r.f.smooth_draws, 0);
    CHECK(drew(&r.f, "Perspective Correction"));
    CHECK(drew(&r.f, "Full screen"));
    CHECK(r.f.title_draws > 0);
}

/* 4: View Distance offers four values, "Ultra" among them: the fourth is the port's, and it turns view_ultra on */
static void test_view_distance_has_ultra(void)
{
    PortSettings s = port_settings_defaults();
    int16_t view = 0;
    char out[16];
    graphics_own_view_value(view, s.view_ultra, out);
    CHECK(strcmp(out, "Far") == 0);
    /* three steps from far: medium, close, ultra */
    for (int i = 0; i < 3; i++) graphics_own_view_step(&view, &s.view_ultra);
    CHECK_EQ_INT(s.view_ultra, 1);
    graphics_own_view_value(view, s.view_ultra, out);
    CHECK(strcmp(out, "Ultra") == 0);
    /* and on to far again, which turns it off */
    graphics_own_view_step(&view, &s.view_ultra);
    CHECK_EQ_INT(s.view_ultra, 0);
    graphics_own_view_value(view, s.view_ultra, out);
    CHECK(strcmp(out, "Far") == 0);
}

/* 4: the View Distance row is the fourth, and stepping it on the screen walks view_ultra */
static void test_view_distance_cycles(void)
{
    Rig r;
    rig_init(&r);
    /* down to row 3 and three steps to the right */
    run_script(&r, script_walk(GRAPHICS_OWN_VIEW, RIGHT, 3, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(r.port.view_ultra, 1);
    CHECK_EQ_INT(r.f.saves, 1);   /* Done kept it, so the four steps are what set it */
}

/* 5: the rows write the settings the owner named, each one from its own row */
static void test_values_write_settings(void)
{
    /* Scaling (row 2) toggles scale_integer */
    Rig scaling;
    rig_init(&scaling);
    run_script(&scaling, script_walk(GRAPHICS_OWN_SCALING, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(scaling.port.scale_integer, 1);

    /* Full screen (row 8) takes the desktop's format */
    Rig full;
    rig_init(&full);
    run_script(&full, script_walk(GRAPHICS_OWN_FULLSCREEN, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(full.port.fullscreen, 1);
    CHECK_EQ_INT(full.port.format, SCREEN_21_9);

    /* Refresh rate (row 7) steps 60 -> 120 */
    Rig refresh;
    rig_init(&refresh);
    run_script(&refresh, script_walk(GRAPHICS_OWN_REFRESH, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(refresh.port.refresh_hz, 120);

    /* Field of view (row 5) steps from the default 0 up to +5 */
    Rig fov;
    rig_init(&fov);
    run_script(&fov, script_walk(GRAPHICS_OWN_FOV, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(fov.port.fov_adjust, 5);

    /* Perspective correction (row 0), texture filter (row 1) and fog (row 4) turn on */
    Rig perspective;
    rig_init(&perspective);
    run_script(&perspective, script_walk(GRAPHICS_OWN_PERSPECTIVE, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(perspective.port.perspective, 1);

    Rig filter;
    rig_init(&filter);
    run_script(&filter, script_walk(GRAPHICS_OWN_FILTER, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(filter.port.texture_filter, 1);

    Rig fog;
    rig_init(&fog);
    run_script(&fog, script_walk(GRAPHICS_OWN_FOG, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(fog.port.fog, 1);

    /* Screen format (row 6) steps through its three */
    Rig format;
    rig_init(&format);
    run_script(&format, script_walk(GRAPHICS_OWN_FORMAT, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(format.port.format, SCREEN_16_9);
}

/* 6: Done saves, Cancel puts back, Escape puts back */
static void test_done_saves_cancel_restores(void)
{
    /* Scaling, changed, then Done: to row 2, right, down to Done, choose */
    Rig d;
    rig_init(&d);
    int n = script_walk(GRAPHICS_OWN_SCALING, RIGHT, 1, GRAPHICS_OWN_DONE);
    CHECK_EQ_INT(run_script(&d, n), 1);   /* Done */
    CHECK_EQ_INT(d.f.saves, 1);
    CHECK_EQ_INT(d.f.put_backs, 0);       /* Done keeps the settings, it does not put them back */
    CHECK_EQ_INT(d.port.scale_integer, 1);

    /* the same change, then Cancel: nothing saved, and the settings as they were on entry */
    Rig c;
    rig_init(&c);
    n = script_walk(GRAPHICS_OWN_SCALING, RIGHT, 1, ESCAPE);
    n--;
    for (int i = 0; i < GRAPHICS_OWN_CANCEL - GRAPHICS_OWN_SCALING; i++) script[n++] = DOWN;
    script[n++] = 0;
    script[n++] = SPACE;
    script[n++] = 0;
    script[n++] = ESCAPE;
    CHECK_EQ_INT(run_script(&c, n), 0);   /* Cancel */
    CHECK_EQ_INT(c.f.saves, 0);
    CHECK_EQ_INT(c.f.put_backs, 1);
    CHECK_EQ_INT(c.port.scale_integer, 0);   /* and the settings are as they were on entry */

    /* and Escape, straight away, without touching anything */
    Rig e;
    rig_init(&e);
    script[0] = ESCAPE;
    CHECK_EQ_INT(run_script(&e, 1), 0);
    CHECK_EQ_INT(e.f.saves, 0);
    CHECK_EQ_INT(e.f.put_backs, 1);
}

/* 7: every row the owner named is reachable, and the walk wraps at both ends */
static void test_down_walks_every_row(void)
{
    /* down from the first reaches each row in turn */
    for (int row = 1; row < GRAPHICS_OWN_ROWS; row++) {
        Rig r;
        rig_init(&r);
        const int count = script_walk(row, 0, 0, ESCAPE);   /* no key on the row, only the Escape that ends */
        script[count - 1] = ESCAPE;
        run_script(&r, count - 1);                 /* the walk, without the key that would end it */
        CHECK_EQ_INT(r.f.pointer_row, row);
    }
    /* down from the last wraps to the first */
    Rig wrap;
    rig_init(&wrap);
    int n = 0;
    for (int i = 0; i < GRAPHICS_OWN_ROWS; i++) script[n++] = DOWN;
    script[n++] = ESCAPE;
    run_script(&wrap, n);
    CHECK_EQ_INT(wrap.f.pointer_row, 0);

    /* and up from the first wraps to the last */
    Rig up;
    rig_init(&up);
    script[0] = UP;
    script[1] = ESCAPE;
    run_script(&up, 2);
    CHECK_EQ_INT(up.f.pointer_row, GRAPHICS_OWN_CANCEL);
}

/* 8: FOV's value text */
static void test_fov_value(void)
{
    PortSettings s = port_settings_defaults();
    char out[16];
    graphics_own_value(&s, GRAPHICS_OWN_FOV, out);
    CHECK(strcmp(out, "0") == 0);
    s.fov_adjust = 20;
    graphics_own_value(&s, GRAPHICS_OWN_FOV, out);
    CHECK(strcmp(out, "+20") == 0);
    s.fov_adjust = -10;
    graphics_own_value(&s, GRAPHICS_OWN_FOV, out);
    CHECK(strcmp(out, "-10") == 0);
}

/* 9: every row's value text, pinned one by one */
static void test_value_texts(void)
{
    PortSettings s = port_settings_defaults();
    char out[16];
    graphics_own_value(&s, GRAPHICS_OWN_PERSPECTIVE, out);
    CHECK(strcmp(out, "Off") == 0);
    graphics_own_value(&s, GRAPHICS_OWN_FILTER, out);
    CHECK(strcmp(out, "Off") == 0);
    graphics_own_value(&s, GRAPHICS_OWN_SCALING, out);
    CHECK(strcmp(out, "Fill") == 0);
    graphics_own_view_value(0, s.view_ultra, out);
    CHECK(strcmp(out, "Far") == 0);
    graphics_own_value(&s, GRAPHICS_OWN_FOG, out);
    CHECK(strcmp(out, "Off") == 0);
    graphics_own_value(&s, GRAPHICS_OWN_FORMAT, out);
    CHECK(strcmp(out, "4:3") == 0);
    graphics_own_value(&s, GRAPHICS_OWN_REFRESH, out);
    CHECK(strcmp(out, "60 Hz") == 0);
    graphics_own_value(&s, GRAPHICS_OWN_FULLSCREEN, out);
    CHECK(strcmp(out, "Off") == 0);

    s.view_ultra = 1;
    graphics_own_view_value(0, s.view_ultra, out);
    CHECK(strcmp(out, "Ultra") == 0);
    graphics_own_view_value(1, 0, out);
    CHECK(strcmp(out, "Medium") == 0);
    graphics_own_view_value(2, 0, out);
    CHECK(strcmp(out, "Close") == 0);
    s.scale_integer = 1;
    graphics_own_value(&s, GRAPHICS_OWN_SCALING, out);
    CHECK(strcmp(out, "Whole") == 0);
    s.format = SCREEN_21_9;
    graphics_own_value(&s, GRAPHICS_OWN_FORMAT, out);
    CHECK(strcmp(out, "21:9") == 0);
}

/* 10: a row steps its own setting only */
static void test_step_writes_only_its_row(void)
{
    PortSettings s = port_settings_defaults();
    const PortSettings before = s;
    graphics_own_step(&s, GRAPHICS_OWN_SCALING, 1, SCREEN_16_9);
    CHECK_EQ_INT(s.scale_integer, before.scale_integer ^ 1);
    CHECK_EQ_INT(s.format, before.format);
    CHECK_EQ_INT(s.refresh_hz, before.refresh_hz);
    CHECK_EQ_INT(s.fullscreen, before.fullscreen);
    CHECK_EQ_INT(s.fov_adjust, before.fov_adjust);
    CHECK_EQ_INT(s.view_ultra, before.view_ultra);
}

/* 11: turning full screen on takes the desktop's format */
static void test_fullscreen_takes_desktop_format(void)
{
    PortSettings s = port_settings_defaults();
    s.format = SCREEN_4_3;
    graphics_own_step(&s, GRAPHICS_OWN_FULLSCREEN, 1, SCREEN_21_9);
    CHECK_EQ_INT(s.fullscreen, 1);
    CHECK_EQ_INT(s.format, SCREEN_21_9);
}

/* 12: port: 00423d28 leaves through one door, and only Cancel says no. Its loop ends on Done, on Cancel, and on the
 * idle deadline, the right button or the quit flag -- and everything but Cancel falls through to the same plus_save
 * (graphics_menu.c: put_back is the only thing that sets cancelled). So the three exits that are not the player's
 * decision still save what they changed on the way past, and the screen reports that it left. Each walk changes a
 * setting and then settles, with no Done, no Cancel and no Escape: the trigger, not a key, is what ends it. */

/* one trigger's run: change Scaling, then let `f` end the screen the way that trigger does */
static void test_the_three_exits_that_are_not_a_decision_save(void)
{
    enum { SCALING = GRAPHICS_OWN_SCALING };
    int n;

    /* the idle deadline passing */
    Rig idle;
    rig_init(&idle);
    change_a_setting_then_settle(SCALING, RIGHT, &n);
    idle.f.deadline_passed = 1;
    CHECK_EQ_INT(run_script_idling(&idle, n), 1);   /* it left, and left as Done leaves */
    CHECK_EQ_INT(idle.f.saves, 1);
    CHECK_EQ_INT(idle.f.put_backs, 0);
    CHECK_EQ_INT(idle.port.scale_integer, 1);   /* the change is still in the settings that were saved */

    /* the right button: a mouse button only reaches the screen while the pointer is active, so the rig turns it on */
    Rig right;
    rig_init(&right);
    change_a_setting_then_settle(SCALING, RIGHT, &n);
    right.f.right_button = 1;
    right.pointer_active = 1;
    CHECK_EQ_INT(run_script_idling(&right, n), 1);
    CHECK_EQ_INT(right.f.saves, 1);
    CHECK_EQ_INT(right.f.put_backs, 0);
    CHECK_EQ_INT(right.port.scale_integer, 1);

    /* the quit flag */
    Rig quit;
    rig_init(&quit);
    change_a_setting_then_settle(SCALING, RIGHT, &n);
    CHECK_EQ_INT(run_script_quitting(&quit, n), 1);
    CHECK_EQ_INT(quit.f.saves, 1);
    CHECK_EQ_INT(quit.f.put_backs, 0);
    CHECK_EQ_INT(quit.port.scale_integer, 1);
}

/* 13: port: the screen must never be a black rectangle with letters on it. Options fades the display out before it
 * opens this screen, and only a palette of its own puts anything back on it -- so when the clean background cannot be
 * composed (config_bgnd_load fails, so the glue has no shape and this screen never fades a palette in), the words
 * would be drawn over a black screen. It then runs over the caller's own burnt-in Graphics screen, the picture 00423d28
 * put up, with that screen's palette -- so there is something to read. */
static void test_no_clean_background_runs_over_the_originals(void)
{
    Rig r;
    rig_init(&r);
    r.f.bgnd_ok = 0;   /* the CD screens could not be composed */
    script[0] = ESCAPE;
    run_script(&r, 1);

    CHECK_EQ_INT(r.f.bgnd_calls, 1);        /* it did ask, once, before drawing anything */
    CHECK_EQ_INT(r.f.burnt_in_reads, 1);    /* and it asked for the original's screen, once */
    CHECK_EQ_INT(r.f.fade_ins, 1);          /* so the original's own palette went on the display */
    CHECK_EQ_INT(r.f.pixels_drawn_before_fade_in, 1);   /* and only after the screen was drawn */
    CHECK(r.f.palette_sets > 0);           /* the palette came off the original's own bank */
    CHECK_EQ_INT(r.f.non_raw_modes, 0);     /* and with the mode that bank's header says, as the exe reads it */
}

/* 14: port: no flash on the way in. fade_in ramps whatever is ON the display, not the backbuffer: video_set_palette
 * presents on every step. So the order has to be the caller's picture out in its own palette, the page put up on the
 * black display, and only then this screen's palette faded in -- the run opens O F I, the order extras_menu.c's page
 * takes (fc15abb). Fading in before the flip ramped Options' own art up out of black in this screen's palette for all
 * seventeen steps, and the capture showed it: frames 402-417 climbing while Options' TRANSMISSION/ABS rows were the
 * picture, then 418 a hard cut to this page.
 *
 * This is a display with a menu on it -- nothing has faded it out -- so the fade is the one that has to happen. */
static void test_no_flash_on_the_way_in(void)
{
    Rig r;
    rig_init(&r);
    r.f.dark = 0;   /* a menu is showing: there is a picture to fade out before the page goes up */
    script[0] = ESCAPE;
    run_script(&r, 1);

    /* the page is drawn into the backbuffer before the display is touched, so the flip has something to put up */
    CHECK_EQ_INT(r.f.pixels_drawn_before_fade_in, 1);
    CHECK(r.f.event_count >= 3);
    CHECK_EQ_INT(r.f.events[0], 'O');   /* the menu's picture going out, in its own palette */
    CHECK_EQ_INT(r.f.events[1], 'F');   /* the page onto the black display */
    CHECK_EQ_INT(r.f.events[2], 'I');   /* and the page's own palette up over it */
}

/* port: the walk from Options, where Options has already faded the display out (check_options.c's choose) before it
 * opened this screen. A fade scales the palette and the last step of a fade out is 0, so that display is entirely
 * black -- fading it out again scales zeros for seventeen vblanks, and the player waits through them. One fade on
 * the way out, and the page still goes up on a black display. */
static void test_an_already_black_display_is_not_faded_again(void)
{
    Rig r;
    rig_init(&r);
    r.f.dark = 1;   /* Options faded the display out before opening this screen */
    script[0] = ESCAPE;
    run_script(&r, 1);

    CHECK_EQ_INT(r.f.fade_outs, 1);          /* only the screen's own, as it closes */
    CHECK_EQ_INT(r.f.fade_ins, 1);           /* the page's palette still goes up */
    /* the page is still put up on a black display before the palette ramps over it, or it would flash */
    CHECK(r.f.event_count >= 2);
    CHECK_EQ_INT(r.f.events[0], 'F');
    CHECK_EQ_INT(r.f.events[1], 'I');
}

/* 15: port: and none on the way out either. 00423d28's close_screen fades the whole display out, and the screen
 * leaves with nothing on it; this one left the previous screen showing in this screen's palette. Two fades leave on
 * a lit display: the opener's picture going out as the screen opens, and this screen's as it closes. */
static void test_the_display_is_faded_out_on_the_way_out(void)
{
    Rig r;
    rig_init(&r);
    r.f.dark = 0;
    script[0] = ESCAPE;
    run_script(&r, 1);

    CHECK_EQ_INT(r.f.fade_outs, 2);
    /* and the last of them after the last thing drawn, not before it: a fade_out earlier would blank the screen while
     * it was up */
    CHECK(r.f.event_count >= 2);
    CHECK_EQ_INT(r.f.events[r.f.event_count - 1], 'O');
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

/* 7: the pause menu's Graphics (plus.race): the rows the next race reads are dimmed and not edited there. The rule is
 * a pure one, so it is pinned first, then the screen's own behaviour under it. */
static void test_race_live_rows(void)
{
    /* the live rows: the three that moved to EXTRAS, fog, the field, refresh and the view's three near values */
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_PERSPECTIVE, 0, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_FILTER, 0, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_FOG, 0, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_FOV, 0, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_REFRESH, 0, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_VIEW, 0, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_VIEW, 2, 0), 1);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_VIEW, 1, 1), 1);
    /* the next race's: scaling, the format, full screen, and Ultra (view 0 with the flag) */
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_SCALING, 0, 0), 0);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_FORMAT, 0, 0), 0);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_FULLSCREEN, 0, 0), 0);
    CHECK_EQ_INT(graphics_own_live_in_race(GRAPHICS_OWN_VIEW, 0, 1), 0);
}

/* the view distance in the race goes Far, Medium, Close and back to Far; landing on Far drops the Ultra flag, so the
 * row never sits on a value it cannot leave */
static void test_race_view_steps_without_ultra(void)
{
    int16_t view = 0;
    int ultra = 0;
    graphics_own_race_view_step(&view, &ultra);
    CHECK_EQ_INT(view, 1);
    graphics_own_race_view_step(&view, &ultra);
    CHECK_EQ_INT(view, 2);
    CHECK_EQ_INT(ultra, 0);
    int16_t from_close = 2;
    int flag = 1;
    graphics_own_race_view_step(&from_close, &flag);
    CHECK_EQ_INT(from_close, 0);
    CHECK_EQ_INT(flag, 0);
}

/* the pause menu's walk: a dimmed row does not change, a live one does, and the dimmed ones are drawn dimmed */
static void test_race_dims_next_race_rows(void)
{
    Rig scaling;
    rig_init(&scaling);
    scaling.plus.race = 1;
    run_script(&scaling, script_walk(GRAPHICS_OWN_SCALING, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(scaling.port.scale_integer, 0);
    CHECK(scaling.f.dim_draws > 0);
    CHECK(drew(&scaling.f, "Fill (next race)"));

    Rig format;
    rig_init(&format);
    format.plus.race = 1;
    run_script(&format, script_walk(GRAPHICS_OWN_FORMAT, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(format.port.format, SCREEN_4_3);

    Rig full;
    rig_init(&full);
    full.plus.race = 1;
    run_script(&full, script_walk(GRAPHICS_OWN_FULLSCREEN, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(full.port.fullscreen, 0);

    /* Options draws nothing dimmed and does change scaling */
    Rig options;
    rig_init(&options);
    run_script(&options, script_walk(GRAPHICS_OWN_SCALING, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(options.f.dim_draws, 0);
    CHECK_EQ_INT(options.port.scale_integer, 1);

    /* the live rows change in the race: fog is written and saved */
    Rig fog;
    rig_init(&fog);
    fog.plus.race = 1;
    run_script(&fog, script_walk(GRAPHICS_OWN_FOG, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(fog.port.fog, 1);
    CHECK_EQ_INT(fog.f.saves, 1);

    /* Ultra is dimmed and holds still in the race */
    Rig view;
    rig_init(&view);
    view.plus.race = 1;
    view.port.view_ultra = 1;
    view.f.kept = view.port;
    run_script(&view, script_walk(GRAPHICS_OWN_VIEW, RIGHT, 1, GRAPHICS_OWN_DONE));
    CHECK_EQ_INT(view.port.view_ultra, 1);
    CHECK(drew(&view.f, "Ultra (next race)"));
}

/* Escape in the race puts the settings back, the way Options does */
static void test_race_cancel_puts_back(void)
{
    Rig r;
    rig_init(&r);
    r.plus.race = 1;
    run_script(&r, script_walk(GRAPHICS_OWN_FOG, RIGHT, 1, ESCAPE));
    CHECK_EQ_INT(r.f.put_backs, 1);
    CHECK_EQ_INT(r.port.fog, 0);
    CHECK_EQ_INT(r.f.saves, 0);
}

int main(void)
{
    test_race_live_rows();
    test_race_view_steps_without_ultra();
    test_race_dims_next_race_rows();
    test_race_cancel_puts_back();
    test_rows_in_order();
    test_done_and_cancel_light_up();
    test_title_top_left();
    test_palette_reaches_the_display();
    test_palette_is_not_rewritten_in_place();
    test_palette_is_put_raw();
    test_done_and_cancel_are_not_drawn_twice();
    test_text_is_drawn_over_the_background();
    test_rows_are_drawn_without_a_smooth_op();
    test_view_distance_has_ultra();
    test_view_distance_cycles();
    test_values_write_settings();
    test_done_saves_cancel_restores();
    test_down_walks_every_row();
    test_fov_value();
    test_value_texts();
    test_step_writes_only_its_row();
    test_fullscreen_takes_desktop_format();
    test_the_three_exits_that_are_not_a_decision_save();
    test_no_clean_background_runs_over_the_originals();
    test_no_flash_on_the_way_in();
    test_an_already_black_display_is_not_faded_again();
    test_the_display_is_faded_out_on_the_way_out();
    return TEST_REPORT();
}
