/* The EXTRAS screen (extras_menu.c): two pages of switches driven from config.cfg and to it on Done. The plan's steps
 * are exercised with fake ops and a key script, the same shape as test_graphics_plus.c. */
#include "game/frontend/extras_menu.h"
#include "game/sys/port_extras.h"
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
} Fake;

static int32_t f_key(void *ctx)
{
    Fake *f = ctx;
    return f->key_next < f->key_count ? f->keys[f->key_next++] : ESCAPE;
}
static void f_styles(void *ctx)
{
    Fake *f = ctx;
    f->style_rebuilds++;
    /* the styles are a ramp matched to the palette on the display, so a rebuild that comes before the screen's own
     * palette has gone on rebuilds them from the caller's -- which is a different menu's menu */
    if (f->fade_ins > 0) f->styles_faded++;
}
static int32_t f_ticks(void *ctx) { (void)ctx; return 100; }
static int32_t f_zero(void *ctx) { (void)ctx; return 0; }
static void f_void(void *ctx) { (void)ctx; }
static void f_one(void *ctx, int32_t v) { (void)ctx, (void)v; }
static void f_move(void *ctx, int32_t x, int32_t y) { (void)ctx, (void)x, (void)y; }
static int32_t f_art(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
static const uint8_t *f_shape(void *ctx, int32_t bank, const char *tag)
{
    static uint8_t shape[16];
    (void)ctx, (void)bank, (void)tag;
    return shape;
}
static void f_palette(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx, (void)rgb, (void)first, (void)count, (void)src, (void)mode;
}
static int32_t f_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    Fake *f = ctx;
    return f->pointer_in_nav && x0 == EXTRAS_NAV_X0 && x1 == EXTRAS_NAV_X1 && y0 == EXTRAS_NAV_Y0 && y1 == EXTRAS_NAV_Y1;
}
/* The screen starts a redraw by putting the caller's picture up, so that is where the log of words begins again:
 * the rig keeps the last frame the screen drew, which is the page as it stands, not every page it passed through. */
static void f_select(void *ctx, int32_t picture)
{
    Fake *f = ctx;
    (void)picture;
    f->drawn_count = 0;
    f->smooth_draws = 0;
}
/* the clean background, asked for once as the screen opens (config_bgnd_load takes about half a second) */
static int32_t f_bgnd(void *ctx, uint8_t *pixels, uint8_t *palette)
{
    Fake *f = ctx;
    f->bgnd_asks++;
    memset(pixels, 0x2a, EXTRAS_BGND_W * EXTRAS_BGND_H);
    memset(palette, 0x10, 768);
    return f->bgnd_ok;
}
static int32_t f_bgnd_shape(void *ctx, const uint8_t *pixels, const uint8_t *palette)
{
    Fake *f = ctx;
    (void)pixels, (void)palette;
    f->panel_after_bgnd = 1;   /* the background is down; whatever is drawn next lands on it */
    return 1;
}
static void f_blit(void *ctx, const uint8_t *shape)
{
    (void)shape;
    ((Fake *)ctx)->blits++;
}
static int32_t f_picture(void *ctx, int32_t picture) { (void)ctx; return picture; }
/* the present: from here the frame is on the display, which is what says whether the palette was already on it */
static void f_flip(void *ctx)
{
    Fake *f = ctx;
    f->flips++;
    f->events[f->event_count < 16 ? f->event_count++ : 15] = 'F';
}
static void f_fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)first, (void)count, (void)rgb;
    Fake *f = ctx;
    f->fade_ins++;
    /* where it fell in the run: the page must already be drawn when the palette goes on */
    if (f->blits > 0) f->pixels_drawn_before_fade_in = 1;
    f->events[f->event_count < 16 ? f->event_count++ : 15] = 'I';
}
static void f_fade_out(void *ctx, int32_t first, int32_t count)
{
    (void)first, (void)count;
    Fake *f = ctx;
    f->fade_outs++;
    f->events[f->event_count < 16 ? f->event_count++ : 15] = 'O';
}
static int32_t f_buttons(void *ctx) { (void)ctx; return 0; }
static void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
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
static void f_text_smooth(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    ((Fake *)ctx)->smooth_draws++;
    f_text(ctx, style, text, x, y);
}
static int32_t f_text_width(void *ctx, const char *text) { (void)ctx; return (int32_t)strlen(text) * 10; }
static void f_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    Fake *f = ctx;
    (void)rgb, (void)x0, (void)y0, (void)x1, (void)y1, (void)framed;
    if (f->bgnd_asks == 0) f->panel_before_bgnd++;
    f->panels++;
}
/* the header op: recorded with the words, as the red they replace */
static void f_header(void *ctx, const char *text, int32_t x, int32_t y, int32_t width)
{
    Fake *f = ctx;
    f->headers++;
    f->header_width = width;
    f_text(ctx, 2, text, x, y);
}
/* the title op: recorded with the words, in red */
static void f_title(void *ctx, const char *text, int32_t x, int32_t y)
{
    ((Fake *)ctx)->titles++;
    f_text(ctx, 2, text, x, y);
}
static void f_open_log(void *ctx)
{
    Fake *f = ctx;
    f->log_opens++;
}
static void f_save(void *ctx, const PortExtras *e, int16_t unlock)
{
    Fake *f = ctx;
    f->saves++;
    f->saved = *e;
    f->saved_unlock = unlock;
}

static int shown(const Fake *f, const char *text)
{
    for (int i = 0; i < f->drawn_count; i++)
        if (strcmp(f->drawn[i], text) == 0) return 1;
    return 0;
}

/* the style `text` was drawn in, -1 if it was never drawn */
static int32_t style_in(const Fake *f, const char *text)
{
    for (int i = 0; i < f->drawn_count; i++)
        if (strcmp(f->drawn[i], text) == 0) return f->style_of[i];
    return -1;
}

/* the y `text` was drawn at, -1 if it was never drawn */
static int32_t y_in(const Fake *f, const char *text)
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

typedef struct Rig {
    Fake fake;
    PortExtras e;
    int16_t unlock;
    PortExtras saved;
    int16_t saved_unlock;
} Rig;

static void setup(Rig *r)
{
    memset(r, 0, sizeof *r);
    r->e = port_extras_defaults();
    r->unlock = 0;
    r->fake.bgnd_ok = 1;   /* the CD has the configuration screens */
}

static void run_with(Rig *r, const int *keys, int count, void (*smooth)(void *, int32_t, const char *, int32_t,
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
        .extras_save = f_save, .ctx = &r->fake, .open_log_folder = f_open_log };
    (void)extras_menu_run(&r->e, &r->unlock, 1, &ops);
}

static void run(Rig *r, const int *keys, int count) { run_with(r, keys, count, f_text_smooth); }

/* DOWN `count` times, appended to keys from n on; returns the new count */
static int downs(int *keys, int n, int count)
{
    for (int i = 0; i < count; i++) keys[n++] = DOWN;
    return n;
}

/* the flat list of rows splits into the three pages at ROW_FIRST_P2 and ROW_FIRST_P3 */
static int page_of(int row) { return row < ROW_FIRST_P2 ? 0 : row < ROW_FIRST_P3 ? 1 : 2; }
static int first_on(int page) { return page == 0 ? 0 : page == 1 ? ROW_FIRST_P2 : ROW_FIRST_P3; }
/* a row's index on its own page */
static int index_of(int row) { return row - first_on(page_of(row)); }
/* how many rows its page shows */
static int rows_of_page(int page) { return page == 0 ? ROWS_P1 : page == 1 ? ROWS_P2 : ROWS_P3; }
static int rows_on(int row) { return rows_of_page(page_of(row)); }

/* the DOWNs from a row to its page's Done, which is the row past the page's last switch */
static int downs_to_done(int row) { return rows_on(row) - index_of(row); }

/* the keys from the screen's first row to `row`: for each page before its own, DOWN to Done, RIGHT to Cancel, RIGHT round to the next page's first row; then DOWN to
 * the row. Returns how many keys were written. */
static int keys_to_row(int *keys, int row)
{
    int n = 0;
    for (int page = 0; page < page_of(row); page++) {
        n = downs(keys, n, rows_of_page(page));
        keys[n++] = RIGHT, keys[n++] = RIGHT;
    }
    return downs(keys, n, index_of(row));
}

/* Edit Mirror (on page 1, then Done) turns it on and turns Rally off when Rally was on */
static void test_mirror_toggle_excludes_rally(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_MIRROR);
    keys[n++] = ENTER;                                             /* Mirror on */
    n = downs(keys, n, downs_to_done(ROW_MIRROR));
    keys[n++] = ENTER;                                             /* Done */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    r.e.rally = 1;
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.mirror, 1);
    CHECK_EQ_INT(r.e.rally, 0);
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* Escape restores the input PortExtras */
static void test_escape_restores_input(void)
{
    Rig r;
    const int keys[] = { SPACE, DOWN, SPACE, DOWN, SPACE, ESCAPE };  /* toggle off Explode, Nitro, Turbo and quit */
    setup(&r);
    r.e.explode = 1;
    r.e.nitro = 1;
    r.e.turbo = 1;
    run(&r, keys, 6);
    CHECK_EQ_INT(r.e.explode, 1);
    CHECK_EQ_INT(r.e.nitro, 1);
    CHECK_EQ_INT(r.e.turbo, 1);
}

/* Right on the last row (Cancel) turns to page 2: Down from the first switch round to Done, Right Cancel, Right page 2 */
static void test_right_turns_to_page_two(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_FIRST_P2);
    keys[n++] = ESCAPE;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "< 2/3 >"));
    CHECK(shown(&r.fake, "Speed units"));
}

/* Rally on with Mirror on turns Mirror off (both on page 1) */
static void test_rally_excludes_mirror(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_RALLY);
    keys[n++] = SPACE;                                             /* Rally on */
    n = downs(keys, n, downs_to_done(ROW_RALLY));
    keys[n++] = ENTER;                                             /* Done */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    r.e.mirror = 1;
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.rally, 1);
    CHECK_EQ_INT(r.fake.saved.mirror, 0);
}

/* Warrior Player Car (CHEATS, page 1) and Start changed, Done saves them; Start Screen is the Miscellaneous row */
static void test_page_two_switches_saved(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_WARRIOR);                 /* Warrior Player Car */
    keys[n++] = ENTER;                                     /* on */
    n = downs(keys, n, downs_to_done(ROW_WARRIOR));         /* page 1's Done */
    keys[n++] = RIGHT, keys[n++] = RIGHT;                  /* Done to Cancel, Cancel to page 2's first row */
    n = downs(keys, n, ROWS_P2);                           /* page 2's Done */
    keys[n++] = RIGHT, keys[n++] = RIGHT;                  /* and on to page 3's first row, Start Screen */
    for (int i = 0; i < 3; i++) keys[n++] = LEFT;          /* Start's three values, round back to Game Mode */
    n = downs(keys, n, downs_to_done(ROW_START));          /* down to Done */
    keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.warrior, 1);
    CHECK_EQ_INT((int)r.fake.saved.start, START_GAME_MODE);
}

/* Unlock Level 1 or more gives the Warrior as the exe does (extras_apply.c's settle): the row shows that, dimmed and
 * with its reason, and a toggle does not turn it off behind the player's back */
static void test_warrior_row_is_fixed_by_the_level(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_WARRIOR);
    keys[n++] = SPACE;                                       /* tries to turn it on or off */
    n = downs(keys, n, downs_to_done(ROW_WARRIOR));
    keys[n++] = ENTER;
    setup(&r);
    r.unlock = 1;
    r.e.warrior = 0;
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.warrior, 0);                   /* the player's switch is untouched */
    CHECK(shown(&r.fake, "On (level)"));
    CHECK_EQ_INT(style_in(&r.fake, "On (level)"), STYLE_DIM);
}

/* at level 0 the row is an ordinary switch */
static void test_warrior_row_toggles_at_level_zero(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_WARRIOR);
    keys[n++] = SPACE;
    n = downs(keys, n, downs_to_done(ROW_WARRIOR));
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saved.warrior, 1);
    CHECK(!shown(&r.fake, "On (level)"));
}

/* The three settings that moved to the port's own Graphics screen are not here any more. */
static void test_the_graphics_settings_are_gone(void)
{
    Rig r;
    /* page 2 on Intro video and turn it on; Down to Done */
    int keys[48];
    int n = keys_to_row(keys, ROW_MOVIES);
    keys[n++] = RIGHT;                                     /* off: no movies */
    n = downs(keys, n, downs_to_done(ROW_MOVIES)), keys[n++] = ENTER;   /* down to Done, save */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Intro video"));
    CHECK(!shown(&r.fake, "Modern 3D"));
    CHECK(!shown(&r.fake, "Texture Filter"));
    CHECK(!shown(&r.fake, "Fog"));
    CHECK_EQ_INT(r.fake.saved.no_movies, 1);
    /* nothing on either page may write them any more */
    CHECK_EQ_INT(r.fake.saved.modern_3d, 0);
    CHECK_EQ_INT(r.fake.saved.texture_filter, 0);
    CHECK_EQ_INT(r.fake.saved.fog, 0);
}

/* port: the screen opens over the configuration screens' clean background, not over the caller's picture: asked
 * for once, put on before the dark panel shades over it. */
static void test_clean_background_is_under_the_panel(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK_EQ_INT(r.fake.bgnd_asks, 1);
    CHECK_EQ_INT(r.fake.panel_after_bgnd, 1);
    CHECK_EQ_INT(r.fake.panel_before_bgnd, 0);
}

/* with no background to be had (config_bgnd_load failed) the screen still runs, over the caller's picture */
static void test_no_background_still_runs(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    r.fake.bgnd_ok = 0;
    run(&r, keys, 1);
    CHECK_EQ_INT(r.fake.panel_after_bgnd, 0);
    CHECK_EQ_INT(r.fake.panel_before_bgnd, 0);
}

/* port: no flash on the way in. fade_in ramps whatever the display shows, so the opener's picture goes out first in its
 * own palette, the page is presented on the black display, and only then is this screen's palette faded in: the run
 * opens O F I. The page is whole before the flip. (Fading in before the flip ramped Game Mode's art up again first,
 * then cut to this page -- the capture showed it.) */
static void test_no_flash_on_the_way_in(void)
{
    Rig r;
    const int keys[] = { DOWN, ESCAPE };
    setup(&r);
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.pixels_drawn_before_fade_in, 1);
    CHECK(r.fake.event_count >= 3);
    CHECK_EQ_INT(r.fake.events[0], 'O');
    CHECK_EQ_INT(r.fake.events[1], 'F');
    CHECK_EQ_INT(r.fake.events[2], 'I');
}

/* port: and none on the way out either. Every screen that opens one of these fades the whole display out as it leaves,
 * the way 00423d28's close_screen does, so the menu the screen was opened from comes back up in its own palette
 * instead of reading this screen's. */
static void test_the_display_is_faded_out_on_the_way_out(void)
{
    Rig r;
    const int keys[] = { DOWN, ESCAPE };
    setup(&r);
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.fade_outs, 2);   /* the opener's picture going out, then this screen's */
    /* and after the last thing drawn, not before it: a fade_out earlier would blank the screen while it was up */
    CHECK(r.fake.event_count >= 2);
    CHECK_EQ_INT(r.fake.events[r.fake.event_count - 1], 'O');
}

/* port: the letters are drawn in a ramp matched to the palette on the display, so the styles can only be rebuilt once
 * this screen's own palette has gone on. The old order asked for them as the screen opened -- before the fade -- which
 * built them from the caller's palette: Options' rows came out in the exe's colours over the clean background, and on
 * the way out the rebuild took that same palette with it. */
static void test_the_styles_are_rebuilt_after_the_palette_is_on(void)
{
    Rig r;
    const int keys[] = { DOWN, ESCAPE };
    setup(&r);
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.style_rebuilds, 2);      /* the screen's palette going up, and the caller's coming back */
    CHECK_EQ_INT(r.fake.styles_faded, 2);        /* both of them after the screen's palette was on the display */
}

/* port: no clean background to put a palette on (config_bgnd_load fails), so the screen stays over the caller's live
 * picture -- and then it must not fade the display out at all, or the caller comes back to a black screen. The
 * Graphics screen had exactly this defect: it had no palette of its own to fall back to and faded anyway. */
static void test_no_background_leaves_the_display_alone(void)
{
    Rig r;
    const int keys[] = { DOWN, ESCAPE };
    setup(&r);
    r.fake.bgnd_ok = 0;
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.fade_ins, 0);
    CHECK_EQ_INT(r.fake.fade_outs, 0);
}

/* CHEATS and UNLOCKS & MODS on page 1: thirteen switches, Car Colours the last one. Walk the whole page down
 * and they are all of them */
static void test_page_one_has_thirteen_rows(void)
{
    Rig r;
    int keys[48];
    int n = downs(keys, 0, ROWS_P1);                       /* down to Done, past the thirteen */
    keys[n++] = ENTER;
    CHECK_EQ_INT(ROWS_P1, 13);
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Explode mode"));
    CHECK(shown(&r.fake, "Nitro mode"));
    CHECK(shown(&r.fake, "Turbo mode"));
    CHECK(shown(&r.fake, "Warrior Player Car"));
    CHECK(shown(&r.fake, "Warrior Rival Car"));
    CHECK(shown(&r.fake, "Mirror track mode"));
    CHECK(shown(&r.fake, "Rally track mode"));
    CHECK(shown(&r.fake, "Lost Vegas Track"));
    CHECK(shown(&r.fake, "Lunar Springs Track"));
    CHECK(shown(&r.fake, "Unlock Level"));
    CHECK(shown(&r.fake, "Time with variants"));
    CHECK(shown(&r.fake, "Car Colours"));
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* port: Car Colours is the last row of Unlocks & mods (page 1): Enter turns it on, and Done saves it, the rest as it was */
static void test_psx_livery_row_is_saved_on_done(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_PSX_LIVERY);
    keys[n++] = ENTER;                                     /* on */
    n = downs(keys, n, downs_to_done(ROW_PSX_LIVERY));     /* down to Done */
    keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.psx_liveries, 1);
    CHECK_EQ_INT(r.fake.saved.no_movies, 0);
}

/* IN-RACE OPTIONS and MISCELLANEOUS on page 2: thirteen rows, Open the log folder the last of them. Walk the whole page down */
static void test_page_two_has_ten_rows(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_FIRST_P2);
    n = downs(keys, n, ROWS_P2);                           /* down to Done (the row past the eleven) */
    keys[n++] = ENTER;
    CHECK_EQ_INT(ROWS_P2, 10);
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Speed units"));
    CHECK(shown(&r.fake, "No driving assists"));
    CHECK(shown(&r.fake, "Bumper view mirror"));
    CHECK(shown(&r.fake, "Bumper view police radar"));
    CHECK(shown(&r.fake, "Replay cameras in race mode"));
    CHECK(shown(&r.fake, "Rival car markers"));
    CHECK(shown(&r.fake, "3D render stats"));
    CHECK(shown(&r.fake, "Dash box"));
    CHECK(shown(&r.fake, "360 Camera"));
    CHECK(shown(&r.fake, "Difficulty"));
    CHECK(!shown(&r.fake, "Start Screen"));   /* the Miscellaneous section is page 3's */
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* page 3 is the whole Miscellaneous section: Start Screen, Intro video, Updates and anonymous stats, Open the log folder; four rows */
static void test_page_three_is_miscellaneous(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_FIRST_P3);
    n = downs(keys, n, ROWS_P3);
    keys[n++] = ENTER;
    CHECK_EQ_INT(ROWS_P3, 4);
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Miscellaneous"));
    CHECK(shown(&r.fake, "Start Screen"));
    CHECK(shown(&r.fake, "Intro video"));
    CHECK(shown(&r.fake, "Updates and anonymous stats"));
    CHECK(shown(&r.fake, "Open the log folder"));
    CHECK(!shown(&r.fake, "Speed units"));
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* port: the two race switches read On / Off from extras.cfg's values (the markers On by default, the overlay Off), and
 * Done saves a flip of the markers row, the rest of the page as it was */
static void test_race_switch_rows_save_with_done(void)
{
    Rig r;
    int keys[48] = { 0 };
    int n = keys_to_row(keys, ROW_MARKERS);                /* Rival car markers, page 2 */
    keys[n++] = ENTER;                                     /* Car markers: On -> Off */
    n = downs(keys, n, downs_to_done(ROW_MARKERS));        /* past 3D render stats and Dash box, down to Done */
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Off"));
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.car_markers, 0);
    CHECK_EQ_INT(r.fake.saved.stats_overlay, 0);
    CHECK_EQ_INT(r.fake.saved.start, START_GAME_MODE);
}

/* the keys that walk from the screen's first row to its Speed units row (page 2's first row) */
static int keys_to_speed_units(int *keys)
{
    return keys_to_row(keys, ROW_UNITS);
}

/* port: the units row sits in IN-RACE OPTIONS, above the switches under it, and reads "mph" while the language is
 * English */
static void test_speed_units_row_reads_mph_in_english(void)
{
    Rig r;
    int keys[48] = { 0 };
    const int n = keys_to_speed_units(keys);
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Speed units"));
    CHECK(shown(&r.fake, "mph"));
    CHECK(!shown(&r.fake, "km/h"));
    CHECK_EQ_INT(style_in(&r.fake, "Speed units"), STYLE_YELLOW);   /* the selection is on it */
    CHECK(y_in(&r.fake, "Speed units") < y_in(&r.fake, "3D render stats"));
}

/* Enter on the units row, from the default, turns the English units to km/h, and Done saves that */
static void test_speed_units_toggle_saves_kmh(void)
{
    Rig r;
    int keys[48] = { 0 };
    int n = keys_to_speed_units(keys);
    keys[n++] = ENTER;
    n = downs(keys, n, downs_to_done(ROW_UNITS));          /* down to Done */
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "km/h"));
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.units, UNITS_KMH);
    CHECK_EQ_INT(r.fake.saved.start, START_GAME_MODE);   /* the rest of the screen is as it was */
}

/* port: the owner's four groups, as the two headers over them on each page, in the owner's names. */
static void test_the_groups_have_headers(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK(shown(&r.fake, "Cheats"));
    CHECK(shown(&r.fake, "Unlocks & mods"));

    Rig two;
    setup(&two);
    int turn[48];
    int n = keys_to_row(turn, ROW_FIRST_P2);
    turn[n++] = ESCAPE;
    run(&two, turn, n);
    CHECK(shown(&two.fake, "In-race options"));
    CHECK(!shown(&two.fake, "Miscellaneous"));   /* it heads page 3 now */
    /* page 1's headers are not page 2's: the two pages name different groups, and each redraw paints the whole page,
     * so what is left on screen after turning is page 2 alone */
    CHECK(!shown(&two.fake, "Cheats"));
    CHECK(!shown(&two.fake, "Unlocks & mods"));
}

/* the headers are the title's red, the rows cream or yellow as they are lit: the headers name a group, they are not a
 * row, so they never take the selection. */
static void test_headers_are_red_and_never_lit(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK_EQ_INT(style_in(&r.fake, "Cheats"), STYLE_RED);
    CHECK_EQ_INT(style_in(&r.fake, "Unlocks & mods"), STYLE_RED);
    CHECK_EQ_INT(style_in(&r.fake, "Explode mode"), STYLE_YELLOW);   /* the first row is selected */
    CHECK_EQ_INT(style_in(&r.fake, "Nitro mode"), STYLE_CREAM);
}

/* The rows sit under the header of their group, on each page: a group's rows are the ones between its header and the
 * next one (or the Done band). */
static void test_the_rows_sit_under_their_groups(void)
{
    Rig one;
    setup(&one);
    const int here[] = { ESCAPE };
    run(&one, here, 1);
    CHECK(y_in(&one.fake, "Cheats") < y_in(&one.fake, "Explode mode"));
    CHECK(y_in(&one.fake, "Explode mode") < y_in(&one.fake, "Warrior Player Car"));
    CHECK(y_in(&one.fake, "Warrior Player Car") < y_in(&one.fake, "Unlocks & mods"));
    CHECK(y_in(&one.fake, "Unlocks & mods") < y_in(&one.fake, "Warrior Rival Car"));
    CHECK(y_in(&one.fake, "Warrior Rival Car") < y_in(&one.fake, "Unlock Level"));
    CHECK(y_in(&one.fake, "Unlock Level") < y_in(&one.fake, "Time with variants"));

    Rig two;
    setup(&two);
    int turn[48];
    int n = keys_to_row(turn, ROW_FIRST_P2);
    turn[n++] = ESCAPE;
    run(&two, turn, n);
    CHECK(y_in(&two.fake, "In-race options") < y_in(&two.fake, "Speed units"));
    CHECK(y_in(&two.fake, "Speed units") < y_in(&two.fake, "3D render stats"));
    CHECK(y_in(&two.fake, "3D render stats") < y_in(&two.fake, "Dash box"));
    CHECK(y_in(&two.fake, "Dash box") < y_in(&two.fake, "Difficulty"));
    Rig three;
    setup(&three);
    n = keys_to_row(turn, ROW_FIRST_P3);
    turn[n++] = ESCAPE;
    run(&three, turn, n);
    CHECK(y_in(&three.fake, "Miscellaneous") < y_in(&three.fake, "Start Screen"));
    CHECK(y_in(&three.fake, "Start Screen") < y_in(&three.fake, "Intro video"));
    CHECK(y_in(&three.fake, "Intro video") < y_in(&three.fake, "Updates and anonymous stats"));
    CHECK(y_in(&three.fake, "Updates and anonymous stats") < y_in(&three.fake, "Open the log folder"));
}

/* The headers sit in the same place on both pages, the rows are one pitch apart, and there is a visible gap before the
 * second group: the same layout for both pages, which is what makes them read as two columns of one list. */
static void test_both_pages_share_one_layout(void)
{
    Rig one;
    setup(&one);
    const int here[] = { ESCAPE };
    run(&one, here, 1);

    Rig two;
    setup(&two);
    int turn[48];
    int n = keys_to_row(turn, ROW_FIRST_P2);
    turn[n++] = ESCAPE;
    run(&two, turn, n);

    /* the first header of each page is line 0; the second header is line 5 on page 1 and line 11 on page 2 (the lines
     * are PITCH apart, with the gap of 14 added from the second header down) */
    CHECK_EQ_INT(y_in(&two.fake, "In-race options"), y_in(&one.fake, "Cheats"));
    CHECK_EQ_INT(y_in(&one.fake, "Unlocks & mods"), 100 + 5 * PITCH + 14);
    Rig three;
    setup(&three);
    n = keys_to_row(turn, ROW_FIRST_P3);
    turn[n++] = ESCAPE;
    run(&three, turn, n);
    CHECK_EQ_INT(y_in(&three.fake, "Miscellaneous"), y_in(&one.fake, "Cheats"));   /* pages 2 and 3 have one header, on the first line */
    CHECK_EQ_INT(y_in(&three.fake, "Start Screen"), y_in(&two.fake, "Speed units"));
    /* the pitch: one row a line below the last */
    CHECK_EQ_INT(y_in(&one.fake, "Nitro mode") - y_in(&one.fake, "Explode mode"), PITCH);
    CHECK_EQ_INT(y_in(&two.fake, "No driving assists") - y_in(&two.fake, "Speed units"), PITCH);
    /* the gap: the second header starts further down than the first group's last row, by more than a row's pitch */
    CHECK_EQ_INT(y_in(&one.fake, "Unlocks & mods") - y_in(&one.fake, "Warrior Player Car"), PITCH + 14);
}

/* The page must fit between the title and the DONE/CANCEL band without covering either: the last row's text has to end
 * above the background's own buttons (430), and the first header has to start below the title. */
static void test_the_page_fits_between_the_title_and_the_buttons(void)
{
    enum { TITLE_BOTTOM = 96, BUTTONS_TOP = 430 };
    Rig one;
    setup(&one);
    const int here[] = { ESCAPE };
    run(&one, here, 1);
    /* the page's first and last words, as the text's own top and its bottom */
    CHECK(y_in(&one.fake, "Cheats") >= TITLE_BOTTOM);
    CHECK(y_in(&one.fake, "Car Colours") + LINE < BUTTONS_TOP);
    CHECK(y_in(&one.fake, "Time with variants") < y_in(&one.fake, "Car Colours"));   /* the group's last row */

    Rig two;
    setup(&two);
    int turn[48];
    int n = keys_to_row(turn, ROW_FIRST_P2);
    turn[n++] = ESCAPE;
    run(&two, turn, n);
    CHECK(y_in(&two.fake, "In-race options") >= TITLE_BOTTOM);
    CHECK(y_in(&two.fake, "Difficulty") + LINE < BUTTONS_TOP);
    Rig three;
    setup(&three);
    n = keys_to_row(turn, ROW_FIRST_P3);
    turn[n++] = ESCAPE;
    run(&three, turn, n);
    CHECK(y_in(&three.fake, "Open the log folder") + LINE < BUTTONS_TOP);
}

/* port: the screen names itself, in the words the player just clicked in Game Mode and Options. It sits on the band's
 * the clean background leaves the burnt-in title in, so it reads as that title. */
static void test_the_screen_is_titled_customize_game(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK(shown(&r.fake, "Customize Game"));
}

/* port: the title in the original screens' look, through its own op (red, at their size), not as a row */
static void test_the_title_goes_through_the_title_op(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK(r.fake.titles > 0);
    CHECK_EQ_INT(style_in(&r.fake, "Customize Game"), 2);
}

/* port: "Intro video" reads On by default (no_movies 0) and Off once the movies are switched off */
static void test_intro_video_reads_the_other_way(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_MOVIES);
    keys[n++] = ESCAPE;
    setup(&r);
    run(&r, keys, n);
    const int32_t y = y_in(&r.fake, "Intro video");
    int on = 0, off = 0;
    for (int i = 0; i < r.fake.drawn_count; i++)
        if (r.fake.y_of[i] == y) on += strcmp(r.fake.drawn[i], "On") == 0, off += strcmp(r.fake.drawn[i], "Off") == 0;
    CHECK(on > 0);
    CHECK_EQ_INT(off, 0);
}

/* port: the groups' headers in the credits' look, underlined across the rows, labels to values */
static void test_the_headers_go_through_the_header_op(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK_EQ_INT(r.fake.headers, 2);
    CHECK_EQ_INT(r.fake.header_width, 600 - 16);
}

/* port: the words go straight onto the clean background, no dark panel; only over the caller's picture (no
 * background to compose) does the panel keep them readable */
static void test_a_panel_only_without_the_clean_background(void)
{
    Rig r;
    const int keys[] = { DOWN, ESCAPE };
    setup(&r);
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.panels, 0);
    Rig bare;
    setup(&bare);
    bare.fake.bgnd_ok = 0;
    run(&bare, keys, 2);
    CHECK(bare.fake.panels > 0);
}

/* port: the screen's own palette reaches the display. Options fades out before opening it and only fade_in puts one
 * back, so without it the whole page draws on black. Once, not per redraw: it is the same palette every time. */
static void test_the_palette_reaches_the_display(void)
{
    Rig r;
    const int keys[] = { DOWN, ESCAPE };
    setup(&r);
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.fade_ins, 1);
}

/* port: the styles are rebuilt for the screen's own palette as it opens, so the letters are drawn with a ramp matched
 * to the background that is showing, and the caller's are rebuilt on the way out. */
static void test_the_styles_are_built_for_the_screens_palette(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK_EQ_INT(r.fake.style_rebuilds, 2);   /* the screen's palette going up, the caller's coming back */
}

/* port: the words go through the screen's smooth draw (the menu art's letters), the nav included; only the title has
 * its own op. Every redraw repaints the whole page first, so nothing has to be put back under them. */
static void test_text_is_drawn_over_the_background(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK(r.fake.smooth_draws > 0);
    CHECK_EQ_INT(r.fake.smooth_draws + r.fake.titles + r.fake.headers, r.fake.drawn_count);
}

/* port: without a smooth op to call the screen still draws every word, the way the original's rows do. */
static void test_rows_are_drawn_without_a_smooth_op(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run_with(&r, keys, 1, NULL);
    CHECK_EQ_INT(r.fake.smooth_draws, 0);
    CHECK(shown(&r.fake, "Cheats"));
    CHECK(shown(&r.fake, "Explode mode"));
}

/* port: the buttons walk across the pages the way they did with three: Right on Cancel turns to the next page, Left
 * on page 2's Done goes back to page 1's Cancel, and Left again lands on page 1's Done, which saves. */
static void test_left_on_page_two_done_goes_back(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_FIRST_P2);               /* page 2, its first row */
    n = downs(keys, n, ROWS_P2);                           /* page 2's Done */
    keys[n++] = LEFT;                                      /* page 1's Cancel */
    keys[n++] = LEFT;                                      /* page 1's Done */
    keys[n++] = ENTER;                                     /* save */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* Up from Cancel is the last row (Open the log folder), and the one over it the last switch (Updates and anonymous stats) */
static void test_up_from_cancel_reaches_the_last_switch(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_FIRST_P3);               /* page 3, its first row */
    n = downs(keys, n, ROWS_P3);                           /* page 3's Done */
    keys[n++] = RIGHT;                                     /* page 3's Cancel */
    keys[n++] = UP;                                        /* Open the log folder, the last row */
    keys[n++] = UP;                                        /* Updates and anonymous stats, the last switch */
    keys[n++] = RIGHT;                                     /* off */
    keys[n++] = DOWN;                                      /* the log folder row */
    keys[n++] = DOWN;                                      /* Done */
    keys[n++] = ENTER;                                     /* save */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.update_check, 0);
}

/* the Done/Cancel walk: Right from Done is Cancel, as it was before the regrouping, and Cancel saves nothing */
static void test_right_from_done_reaches_cancel(void)
{
    Rig r;
    int keys[48];
    int n = downs(keys, 0, ROWS_P1);                       /* Done */
    keys[n++] = RIGHT;                                     /* Cancel */
    keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 0);
}

/* Enter with the pointer on the page indicator turns the page */
static void test_nav_click_turns_the_page(void)
{
    Rig r;
    const int keys[] = { ENTER, ESCAPE };
    setup(&r);
    r.fake.pointer_in_nav = 1;
    run(&r, keys, 2);
    CHECK(shown(&r.fake, "< 2/3 >"));
}

/* port: PageDown turns the page, as the screen's own words promise (a click on the page indicator or PageDown turns
 * to the next). The screen answers the BIOS code 0x5100, which is what the port's page key is reported as; where
 * that code comes from -- a scripted PGDN read by input_get_key while the page keys are on -- is
 * test_input_page_keys's, and the screen's own caller turning the page keys on is menu_glue.c's. This rig hands
 * the screen the code itself, so it covers the screen's half of it. */
static void test_page_down_turns_the_page(void)
{
    Rig r;
    const int keys[] = { PAGE_DOWN, ESCAPE };
    setup(&r);
    run(&r, keys, 2);
    CHECK(shown(&r.fake, "< 2/3 >"));
}

/* port: page 1's Unlock Level still steps through its three values, under Unlocks & mods */
static void test_unlock_steps_through_three_values(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_UNLOCK);                 /* Unlock Level, below Lunar Springs Track */
    keys[n++] = RIGHT, keys[n++] = RIGHT;                  /* 0 to 2 */
    n = downs(keys, n, downs_to_done(ROW_UNLOCK));         /* Done */
    keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved_unlock, 2);
}

/* Escape on page 1 puts the input back */
static void test_escape_on_page_one_restores(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_RIVAL);
    keys[n++] = SPACE, keys[n++] = ESCAPE;                 /* Warrior Rival Car on, then quit */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.warrior_rival, 0);
    CHECK_EQ_INT(r.fake.saves, 0);
}

/* Lunar Springs Track is the Unlocks row under Lost Vegas: Enter turns it on and Done keeps it */
static void test_lunar_row_is_saved_on_done(void)
{
    Rig r;
    int keys[48] = {0};
    int n = keys_to_row(keys, ROW_LUNAR);                  /* down to Lunar Springs Track */
    keys[n++] = ENTER;
    n = downs(keys, n, downs_to_done(ROW_LUNAR));
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.lunar_springs, 1);
    CHECK_EQ_INT(r.e.vegas, 0);
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* port: 360 Camera, the last row of In-race options: off to start with, Enter turns it on and Done saves it (the
 * file the race reads it from), another Enter turns it off again */
static void test_follow_camera_row_toggles_and_saves(void)
{
    Rig r;
    int keys[48] = {0};
    int n = keys_to_row(keys, ROW_FOLLOW_CAMERA);
    keys[n++] = ENTER;
    n = downs(keys, n, downs_to_done(ROW_FOLLOW_CAMERA));
    keys[n++] = ENTER;
    CHECK_EQ_INT(ROW_FOLLOW_CAMERA, EXTRAS_ROWS - 6);   /* before Difficulty, the last row of In-race options, and Miscellaneous' four */
    setup(&r);
    CHECK_EQ_INT(r.e.follow_camera, 0);
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.follow_camera, 1);
    CHECK_EQ_INT(r.fake.saved.follow_camera, 1);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK(shown(&r.fake, "On"));

    Rig again;
    setup(&again);
    again.e.follow_camera = 1;
    run(&again, keys, n);
    CHECK_EQ_INT(again.e.follow_camera, 0);
    CHECK_EQ_INT(again.fake.saved.follow_camera, 0);
}

/* Escape puts 360 Camera back like the other rows */
static void test_follow_camera_row_escape_restores(void)
{
    Rig r;
    int keys[48] = {0};
    int n = keys_to_row(keys, ROW_FOLLOW_CAMERA);
    keys[n++] = SPACE, keys[n++] = ESCAPE;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.follow_camera, 0);
    CHECK_EQ_INT(r.fake.saves, 0);
}

/* port: Difficulty, the last row of In-race options: Enter / Right step Easy -> Original -> Hard and round, Left goes
 * back, Done saves the number the race reads (0 Easy, 1 Original, 2 Hard), and Escape puts it back */
static void test_difficulty_row_cycles_saves_and_escapes(void)
{
    Rig r;
    int keys[48] = {0};
    int n = keys_to_row(keys, ROW_DIFFICULTY);
    keys[n++] = RIGHT;                                     /* Original -> Hard */
    n = downs(keys, n, downs_to_done(ROW_DIFFICULTY));
    keys[n++] = ENTER;
    setup(&r);
    r.e.difficulty = 1;
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.difficulty, 2);
    CHECK_EQ_INT(r.fake.saved.difficulty, 2);
    CHECK(shown(&r.fake, "Difficulty"));
    CHECK(shown(&r.fake, "Hard"));

    Rig wrap;                                              /* Hard -> Easy on Right, Easy -> Hard on Left */
    int k2[48] = {0};
    int m = keys_to_row(k2, ROW_DIFFICULTY);
    k2[m++] = RIGHT;
    m = downs(k2, m, downs_to_done(ROW_DIFFICULTY));
    k2[m++] = ENTER;
    setup(&wrap);
    wrap.e.difficulty = 2;
    run(&wrap, k2, m);
    CHECK_EQ_INT(wrap.e.difficulty, 0);
    CHECK(shown(&wrap.fake, "Easy"));
    k2[keys_to_row(k2, ROW_DIFFICULTY)] = LEFT;
    setup(&wrap);
    wrap.e.difficulty = 0;
    run(&wrap, k2, m);
    CHECK_EQ_INT(wrap.e.difficulty, 2);

    Rig esc;
    int k3[48] = {0};
    int q = keys_to_row(k3, ROW_DIFFICULTY);
    k3[q++] = RIGHT, k3[q++] = ESCAPE;
    setup(&esc);
    esc.e.difficulty = 1;
    run(&esc, k3, q);
    CHECK_EQ_INT(esc.e.difficulty, 1);
    CHECK_EQ_INT(esc.fake.saves, 0);
}

/* Done writes the on-disk file (saves++ in the rig) */
static void test_done_writes_extras_cfg(void)
{
    Rig r;
    /* the first row (Explode) is the initial selection, so ROWS_P1 DOWNs land on Done. */
    int keys[48] = {0};
    int n = downs(keys, 0, ROWS_P1);
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.saved_unlock, 0);
}

/* The CD's files are not read by this test (the tables start as the built-in defaults). */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path; (void)size; return NULL; }

/* Updates and anonymous stats: On by default, Right turns it Off and Done saves it */
static void test_check_for_updates_switch(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_UPDATE);
    keys[n++] = RIGHT;                                     /* off */
    n = downs(keys, n, downs_to_done(ROW_UPDATE)), keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    CHECK_EQ_INT(r.e.update_check, 1);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Updates and anonymous stats"));
    CHECK_EQ_INT(r.fake.saved.update_check, 0);
}

/* port (bug reports): Open the log folder is an action row, the last of page 3's Miscellaneous. Return runs the op once; the arrows do not; Done still saves */
static void test_extras_has_open_log_folder_row(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_LOG_FOLDER);
    keys[n++] = ENTER;
    keys[n++] = RIGHT, keys[n++] = LEFT;                     /* the arrows turn nothing on an action */
    n = downs(keys, n, downs_to_done(ROW_LOG_FOLDER));
    keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Open the log folder"));
    CHECK(shown(&r.fake, "Miscellaneous"));
    CHECK_EQ_INT(r.fake.log_opens, 1);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(ROW_LOG_FOLDER, EXTRAS_ROWS - 1);
}

/* without the op the row does nothing and the screen goes on */
static void test_the_log_folder_row_without_the_op(void)
{
    Rig r;
    int keys[48];
    int n = keys_to_row(keys, ROW_LOG_FOLDER);
    keys[n++] = ENTER;
    keys[n++] = DOWN, keys[n++] = ENTER;
    setup(&r);
    r.fake.keys = keys;
    r.fake.key_count = n;
    ExtrasMenuOps ops = { .pointer_move_to = f_move, .load_art = f_art, .shape = f_shape, .palette_set = f_palette, .pointer_in_rect = f_in_rect, .select = f_select,
                          .blit_own = f_blit, .picture_shape = f_picture, .select_default = f_void, .pointer_hide = f_void, .flip = f_flip, .pointer_show = f_void,
                          .pointer_buttons = f_buttons, .text_styles = f_styles, .fade_in = f_fade_in, .deadline_set = f_one, .fade_out = f_fade_out,
                          .deadline_passed = f_zero, .sound = f_one, .ticks = f_ticks, .pointer_activate_if_moved = f_void, .key = f_key, .pointer_click = f_void,
                          .pointer_off = f_void, .music = f_void, .plus_text = f_text, .plus_text_width = f_text_width, .plus_panel = f_panel, .plus_bgnd = f_bgnd,
                          .plus_bgnd_shape = f_bgnd_shape, .plus_text_smooth = f_text_smooth, .plus_title = f_title, .plus_header = f_header,
                          .extras_save = f_save, .ctx = &r.fake };
    (void)extras_menu_run(&r.e, &r.unlock, 1, &ops);
    CHECK_EQ_INT(r.fake.log_opens, 0);
    CHECK_EQ_INT(r.fake.saves, 1);
}

int main(void)
{
    test_check_for_updates_switch();
    test_mirror_toggle_excludes_rally();
    test_page_three_is_miscellaneous();
    test_extras_has_open_log_folder_row();
    test_the_log_folder_row_without_the_op();
    test_escape_restores_input();
    test_right_turns_to_page_two();
    test_rally_excludes_mirror();
    test_warrior_row_is_fixed_by_the_level();
    test_warrior_row_toggles_at_level_zero();
    test_page_two_switches_saved();
    test_the_graphics_settings_are_gone();
    test_page_one_has_thirteen_rows();
    test_page_two_has_ten_rows();
    test_psx_livery_row_is_saved_on_done();
    test_race_switch_rows_save_with_done();
    test_lunar_row_is_saved_on_done();
    test_follow_camera_row_toggles_and_saves();
    test_follow_camera_row_escape_restores();
    test_difficulty_row_cycles_saves_and_escapes();
    test_speed_units_row_reads_mph_in_english();
    test_speed_units_toggle_saves_kmh();
    test_the_groups_have_headers();
    test_headers_are_red_and_never_lit();
    test_the_rows_sit_under_their_groups();
    test_both_pages_share_one_layout();
    test_the_page_fits_between_the_title_and_the_buttons();
    test_the_screen_is_titled_customize_game();
    test_the_title_goes_through_the_title_op();
    test_intro_video_reads_the_other_way();
    test_the_headers_go_through_the_header_op();
    test_a_panel_only_without_the_clean_background();
    test_the_palette_reaches_the_display();
    test_no_flash_on_the_way_in();
    test_the_display_is_faded_out_on_the_way_out();
    test_no_background_leaves_the_display_alone();
    test_the_styles_are_rebuilt_after_the_palette_is_on();
    test_the_styles_are_built_for_the_screens_palette();
    test_text_is_drawn_over_the_background();
    test_rows_are_drawn_without_a_smooth_op();
    test_clean_background_is_under_the_panel();
    test_no_background_still_runs();
    test_left_on_page_two_done_goes_back();
    test_unlock_steps_through_three_values();
    test_up_from_cancel_reaches_the_last_switch();
    test_right_from_done_reaches_cancel();
    test_nav_click_turns_the_page();
    test_page_down_turns_the_page();
    test_escape_on_page_one_restores();
    test_done_writes_extras_cfg();
    return TEST_REPORT();
}
