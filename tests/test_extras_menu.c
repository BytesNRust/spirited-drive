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
enum { STYLE_CREAM = 0, STYLE_YELLOW = 1, STYLE_RED = 2 };

typedef struct Fake {
    const int *keys;
    int key_count, key_next;
    int saves;
    PortExtras saved;
    int16_t saved_unlock;
    char drawn[512][40];
    int drawn_count;
    int32_t style_of[512];   /* the style each word was drawn in */
    int32_t y_of[512];       /* and where, so a column can be checked */
    int smooth_draws;        /* words drawn through the background-restoring (smooth) op */
    int fade_ins;            /* times the screen's own palette was put on the display */
    int style_rebuilds;      /* times the styles were rebuilt for the screen's palette */
    int pointer_in_nav;
    int bgnd_asks;   /* how many times the screen asked for the clean background */
    int panel_after_bgnd;   /* the background was put on the screen */
    int panel_before_bgnd;  /* the dark panel was drawn before it, over the caller's picture */
    int bgnd_ok;          /* what config_bgnd_load gives: 0 as if the CD had none */
} Fake;

static int32_t f_key(void *ctx)
{
    Fake *f = ctx;
    return f->key_next < f->key_count ? f->keys[f->key_next++] : ESCAPE;
}
static void f_styles(void *ctx) { ((Fake *)ctx)->style_rebuilds++; }
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
static void f_blit(void *ctx, const uint8_t *shape) { (void)ctx, (void)shape; }
static int32_t f_picture(void *ctx, int32_t picture) { (void)ctx; return picture; }
static void f_fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)first, (void)count, (void)rgb;
    ((Fake *)ctx)->fade_ins++;
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
        .select_default = f_void, .pointer_hide = f_void, .flip = f_void, .pointer_show = f_void,
        .pointer_buttons = f_buttons, .text_styles = f_styles, .fade_in = f_fade_in, .deadline_set = f_one,
        .deadline_passed = f_zero, .sound = f_one, .ticks = f_ticks, .pointer_activate_if_moved = f_void,
        .key = f_key, .pointer_click = f_void, .pointer_off = f_void, .music = f_void,
        .plus_text = f_text, .plus_text_width = f_text_width, .plus_panel = f_panel,
        .plus_bgnd = f_bgnd, .plus_bgnd_shape = f_bgnd_shape,
        .plus_text_smooth = smooth,
        .extras_save = f_save, .ctx = &r->fake };
    (void)extras_menu_run(&r->e, &r->unlock, 1, &ops);
}

static void run(Rig *r, const int *keys, int count) { run_with(r, keys, count, f_text_smooth); }

/* Plan 3 Task 3 Step 1 — Edit Mirror (then Done) turns it on and turns Rally off when Rally was on */
static void test_mirror_toggle_excludes_rally(void)
{
    Rig r;
    /* ENTER on Mirror flips it on, then ROWS_P1 DOWNs to Done, then ENTER for Done */
    int keys[11];
    int n = 0;
    keys[n++] = ENTER;
    for (int i = 0; i < ROWS_P1; i++) keys[n++] = DOWN;   /* ROWS_P1 downs reach Done (row ROWS_P1) */
    keys[n++] = ENTER;
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
    const int keys[] = { SPACE, SPACE, SPACE, ESCAPE };  /* toggle off Mirror, Nitro, Turbo and quit */
    setup(&r);
    r.e.mirror = 1;
    r.e.nitro = 1;
    r.e.turbo = 1;
    run(&r, keys, 4);
    CHECK_EQ_INT(r.e.mirror, 1);
    CHECK_EQ_INT(r.e.nitro, 1);
    CHECK_EQ_INT(r.e.turbo, 1);
}

/* Right on the last row (Cancel) turns to page 2: Down from the first switch round to Done, Right Cancel, Right page 2 */
static void test_right_turns_to_page_two(void)
{
    Rig r;
    int keys[20];
    int n = 0;
    for (int i = 0; i < ROWS_P1; i++) keys[n++] = DOWN;   /* Done */
    for (int i = 0; i < ROWS_P1 + 1; i++) keys[n++] = RIGHT;  /* Cancel, then round to page 2 */
    keys[n++] = ESCAPE;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "< 2/2"));
    CHECK(shown(&r.fake, "Enable Warrior Player Car"));
}

/* Rally on with Mirror on turns Mirror off */
static void test_rally_excludes_mirror(void)
{
    Rig r;
    int keys[5];
    int n = 0;
    keys[n++] = DOWN;    /* Rally */
    keys[n++] = SPACE;
    keys[n++] = UP, keys[n++] = UP, keys[n++] = ENTER;   /* Mirror, Done */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    r.e.mirror = 1;
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.rally, 1);
    CHECK_EQ_INT(r.fake.saved.mirror, 0);
}

/* page 2: Warrior Player Car and Start changed, Done saves them; the walk back up is Cancel then the last switches */
static void test_page_two_switches_saved(void)
{
    Rig r;
    int keys[31];
    int n = 0;
    for (int i = 0; i < ROWS_P1; i++) keys[n++] = DOWN;    /* page 1's Done */
    for (int i = 0; i < ROWS_P1 + 1; i++) keys[n++] = RIGHT;   /* Done to Cancel, Cancel to page 2, then the toggles */
    keys[n++] = RIGHT;                                     /* Enable Warrior Player Car on, the ninth toggle */
    for (int i = 0; i < ROWS_P2 - 2; i++) keys[n++] = DOWN;   /* down to Start Screen, the first row of Startup */
    for (int i = 0; i < 3; i++) keys[n++] = LEFT;         /* Start's three values: Network, Control Central, Game Mode */
    for (int i = 0; i < 2; i++) keys[n++] = DOWN;         /* up the Startup group, then Done */
    keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.warrior, 1);
    CHECK_EQ_INT((int)r.fake.saved.start, START_GAME_MODE);
}

/* The three settings that moved to the port's own Graphics screen are not here any more. */
static void test_the_graphics_settings_are_gone(void)
{
    Rig r;
    /* page 2 on Disable intro video, the last switch, and turn it on; Down to Done */
    int keys[28];
    int n = 0;
    for (int i = 0; i < ROWS_P1; i++) keys[n++] = DOWN;   /* page 1 Done */
    for (int i = 0; i < ROWS_P1 + 1; i++) keys[n++] = RIGHT;  /* round to page 2 */
    for (int i = 0; i < ROWS_P2 - 1; i++) keys[n++] = DOWN;   /* Disable intro video */
    keys[n++] = RIGHT;                                     /* on */
    keys[n++] = DOWN, keys[n++] = ENTER;                   /* Done, save */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK(shown(&r.fake, "Disable intro video"));
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

/* page 1's nine switches, Replay cameras the last of them: walk the whole page down and they are all of them */
static void test_page_one_has_nine_rows(void)
{
    Rig r;
    const int keys[] = { DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, ENTER };
    setup(&r);
    run(&r, keys, 10);
    CHECK(shown(&r.fake, "Mirror track mode"));
    CHECK(shown(&r.fake, "Rally track mode"));
    CHECK(shown(&r.fake, "Explode mode"));
    CHECK(shown(&r.fake, "Nitro mode"));
    CHECK(shown(&r.fake, "Turbo mode"));
    CHECK(shown(&r.fake, "No driving assists"));
    CHECK(shown(&r.fake, "Bumper view mirror"));
    CHECK(shown(&r.fake, "Bumper view police radar"));
    CHECK(shown(&r.fake, "Replay cameras in race mode"));
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* page 2's seven switches, Disable intro video the last of them: walk the whole page down and they are all of them */
static void test_page_two_has_seven_rows(void)
{
    Rig r;
    const int keys[] = { UP, RIGHT, RIGHT, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, ENTER };
    setup(&r);
    run(&r, keys, 11);
    CHECK(shown(&r.fake, "Enable Warrior Player Car"));
    CHECK(shown(&r.fake, "Enable Warrior Rival Car"));
    CHECK(shown(&r.fake, "Enable Lost Vegas Track"));
    CHECK(shown(&r.fake, "Unlock Level"));
    CHECK(shown(&r.fake, "Time with variants"));
    CHECK(shown(&r.fake, "Start Screen"));
    CHECK(shown(&r.fake, "Disable intro video"));
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* port: the owner's four groups, as the two headers over them. Both headers on both pages, in the owner's names, and
 * page 1's rows under the group they were put in. */
static void test_the_groups_have_headers(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK(shown(&r.fake, "Game modes"));
    CHECK(shown(&r.fake, "Driving & cameras"));

    Rig two;
    setup(&two);
    int turn[32];
    int n = 0;
    for (int i = 0; i < ROWS_P1; i++) turn[n++] = DOWN;
    for (int i = 0; i < ROWS_P1 + 1; i++) turn[n++] = RIGHT;
    turn[n++] = ESCAPE;
    run(&two, turn, n);
    CHECK(shown(&two.fake, "Unlocks"));
    CHECK(shown(&two.fake, "Startup"));
    /* page 1's headers are not page 2's: the two pages name different groups, and each redraw paints the whole page,
     * so what is left on screen after turning is page 2 alone */
    CHECK(!shown(&two.fake, "Game modes"));
    CHECK(!shown(&two.fake, "Driving & cameras"));
}

/* the headers are the title's red, the rows cream or yellow as they are lit: the headers name a group, they are not a
 * row, so they never take the selection. */
static void test_headers_are_red_and_never_lit(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK_EQ_INT(style_in(&r.fake, "Game modes"), STYLE_RED);
    CHECK_EQ_INT(style_in(&r.fake, "Driving & cameras"), STYLE_RED);
    CHECK_EQ_INT(style_in(&r.fake, "Mirror track mode"), STYLE_YELLOW);   /* the first row is selected */
    CHECK_EQ_INT(style_in(&r.fake, "Explode mode"), STYLE_CREAM);
}

/* The headers sit in the same place on both pages, and there is a visible gap before the second group: the same layout
 * for both pages, which is what makes them read as two columns of one list. */
static void test_both_pages_share_one_layout(void)
{
    Rig one;
    setup(&one);
    const int here[] = { ESCAPE };
    run(&one, here, 1);

    Rig two;
    setup(&two);
    int turn[32];
    int n = 0;
    for (int i = 0; i < ROWS_P1; i++) turn[n++] = DOWN;    /* Done */
    for (int i = 0; i < ROWS_P1 + 1; i++) turn[n++] = RIGHT;   /* Cancel, then round to page 2 */
    turn[n++] = ESCAPE;
    run(&two, turn, n);

    CHECK_EQ_INT(y_in(&two.fake, "Unlocks"), y_in(&one.fake, "Game modes"));
    CHECK_EQ_INT(y_in(&two.fake, "Startup"), y_in(&one.fake, "Driving & cameras"));
    /* the gap: the second group starts further down than the first group's last row, by more than a row's pitch */
    CHECK(y_in(&one.fake, "Driving & cameras") - y_in(&one.fake, "Turbo mode")
          > y_in(&one.fake, "Explode mode") - y_in(&one.fake, "Rally track mode"));
}

/* The page must fit between the title and the DONE/CANCEL band without covering either: the last row's text has to end
 * above the background's own buttons, and the first header has to start below the title. */
static void test_the_page_fits_between_the_title_and_the_buttons(void)
{
    enum { TITLE_BOTTOM = 96, BUTTONS_TOP = 428 };
    Rig one;
    setup(&one);
    const int here[] = { ESCAPE };
    run(&one, here, 1);
    /* the page's first and last words, as the text's own top and its bottom */
    CHECK(y_in(&one.fake, "Game modes") >= TITLE_BOTTOM);
    CHECK(y_in(&one.fake, "Replay cameras in race mode") + LINE < BUTTONS_TOP);

    Rig two;
    setup(&two);
    int turn[32];
    int n = 0;
    for (int i = 0; i < ROWS_P1; i++) turn[n++] = DOWN;    /* Done */
    for (int i = 0; i < ROWS_P1 + 1; i++) turn[n++] = RIGHT;   /* Cancel, then round to page 2 */
    turn[n++] = ESCAPE;
    run(&two, turn, n);
    CHECK(y_in(&two.fake, "Unlocks") >= TITLE_BOTTOM);
    CHECK(y_in(&two.fake, "Disable intro video") + LINE < BUTTONS_TOP);
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

/* port: the words go through the draw that puts the background back under them. Every redraw repaints the whole page --
 * the screen blits the clean background whole and then writes over it -- so the letters must land through that draw, or
 * a row the pointer moves past leaves its old edges behind. The nav and the title included: same page, same reason. */
static void test_text_is_drawn_over_the_background(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK(r.fake.smooth_draws > 0);
    CHECK_EQ_INT(r.fake.smooth_draws, r.fake.drawn_count);
}

/* port: without a smooth op to call the screen still draws every word, the way the original's rows do. */
static void test_rows_are_drawn_without_a_smooth_op(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run_with(&r, keys, 1, NULL);
    CHECK_EQ_INT(r.fake.smooth_draws, 0);
    CHECK(shown(&r.fake, "Game modes"));
    CHECK(shown(&r.fake, "Mirror track mode"));
}

/* port: the buttons walk across the pages the way they did with three: Right on Cancel turns to the next page, Left
 * on page 2's Done goes back to page 1's Cancel, and Left again lands on page 1's Done, which saves. */
static void test_left_on_page_two_done_goes_back(void)
{
    Rig r;
    int keys[30];
    int n = 0;
    for (int i = 0; i < ROWS_P1; i++) keys[n++] = DOWN;    /* page 1's Done */
    keys[n++] = RIGHT;                                     /* page 1's Cancel */
    for (int i = 0; i < ROWS_P1 + 1; i++) keys[n++] = RIGHT;   /* round to page 2, Enable Warrior Player Car */
    for (int i = 0; i < ROWS_P2; i++) keys[n++] = DOWN;   /* page 2's Done */
    keys[n++] = LEFT;                                      /* page 1's Cancel */
    keys[n++] = LEFT;                                      /* page 1's Done */
    keys[n++] = ENTER;                                     /* save */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
}

/* Up from Cancel is the last switch (Disable intro video) */
static void test_up_from_cancel_reaches_the_last_switch(void)
{
    Rig r;
    int keys[32];
    int n = 0;
    for (int i = 0; i < ROWS_P1; i++) keys[n++] = DOWN;    /* page 1's Done */
    keys[n++] = RIGHT;                                     /* page 1's Cancel */
    for (int i = 0; i < ROWS_P1 + 1; i++) keys[n++] = RIGHT;   /* round to page 2 */
    for (int i = 0; i < ROWS_P2; i++) keys[n++] = DOWN;    /* page 2's Done */
    keys[n++] = RIGHT;                                     /* page 2's Cancel */
    keys[n++] = UP;                                        /* the last switch, Disable intro video */
    keys[n++] = RIGHT;                                     /* on */
    keys[n++] = DOWN;                                      /* Done */
    keys[n++] = ENTER;                                     /* save */
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved.no_movies, 1);
}

/* the Done/Cancel walk: Right from Done is Cancel, as it was before the regrouping, and Cancel saves nothing */
static void test_right_from_done_reaches_cancel(void)
{
    Rig r;
    int keys[11];
    int n = 0;
    for (int i = 0; i < ROWS_P1; i++) keys[n++] = DOWN;    /* Done */
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
    CHECK(shown(&r.fake, "< 2/2"));
}

/* port: page 2's Unlock Level still steps through its three values, under Unlocks now rather than its own group */
static void test_unlock_steps_through_three_values(void)
{
    Rig r;
    int keys[29];
    int n = 0;
    for (int i = 0; i < ROWS_P1; i++) keys[n++] = DOWN;    /* page 1's Done */
    for (int i = 0; i < ROWS_P1 + 1; i++) keys[n++] = RIGHT;   /* round to page 2 */
    keys[n++] = DOWN, keys[n++] = DOWN, keys[n++] = DOWN;   /* Unlock Level */
    keys[n++] = RIGHT, keys[n++] = RIGHT;                  /* 0 to 2 */
    for (int i = 0; i < ROWS_P2 - 3; i++) keys[n++] = DOWN;   /* Done */
    keys[n++] = ENTER;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.fake.saved_unlock, 2);
}

/* Escape on page 2 puts the input back */
static void test_escape_on_page_two_restores(void)
{
    Rig r;
    int keys[21];
    int n = 0;
    for (int i = 0; i < ROWS_P1; i++) keys[n++] = DOWN;
    for (int i = 0; i < ROWS_P1 + 1; i++) keys[n++] = RIGHT;
    keys[n++] = SPACE, keys[n++] = ESCAPE;
    CHECK(n <= (int)(sizeof keys / sizeof keys[0]));
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.e.warrior, 0);
    CHECK_EQ_INT(r.fake.saves, 0);
}

/* Done writes the on-disk file (saves++ in the rig) */
static void test_done_writes_extras_cfg(void)
{
    Rig r;
    /* Mirror (row 0) is the initial selection, so ROWS_P1 DOWNs land on Done. */
    int keys[16] = {0};
    int n = 0;
    for (int i = 0; i < ROWS_P1; i++) keys[n++] = DOWN;
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.saves, 1);
    CHECK_EQ_INT(r.saved_unlock, 0);
}

int main(void)
{
    test_mirror_toggle_excludes_rally();
    test_escape_restores_input();
    test_right_turns_to_page_two();
    test_rally_excludes_mirror();
    test_page_two_switches_saved();
    test_the_graphics_settings_are_gone();
    test_page_one_has_nine_rows();
    test_page_two_has_seven_rows();
    test_the_groups_have_headers();
    test_headers_are_red_and_never_lit();
    test_both_pages_share_one_layout();
    test_the_page_fits_between_the_title_and_the_buttons();
    test_the_palette_reaches_the_display();
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
    test_escape_on_page_two_restores();
    test_done_writes_extras_cfg();
    return TEST_REPORT();
}
