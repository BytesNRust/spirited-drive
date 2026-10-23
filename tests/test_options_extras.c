/* The Options screen's Extras entry (options_menu.c, Plan 3 Task 3): a text row drawn over a free band as the last
 * element before Done, reached with the arrows and the pointer, opening the EXTRAS screen without leaving Options. */
#include "game/frontend/options_menu.h"
#include "test.h"

#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, SPACE = 0x20, UP = 0x4800, DOWN = 0x5000 };

typedef struct Fake {
    const int *keys;
    int key_count, key_next;
    int extras_opens;
    int selected_when_opened;
    int pointer_x, pointer_y;
    int pointer_pinned;   /* a test parks the pointer; move_to no longer drags it */
    int moved_y[16];      /* the pointer's y moved to after each key, by the key's index */
    char drawn[128][32];
    int drawn_count;
    int text_x, text_y;   /* where the last text went */
} Fake;

static int32_t f_key(void *ctx)
{
    Fake *f = ctx;
    return f->key_next < f->key_count ? f->keys[f->key_next++] : ESCAPE;
}
static void f_move(void *ctx, int32_t x, int32_t y)
{
    Fake *f = ctx;
    if (f->key_next > 0 && f->key_next <= 16) f->moved_y[f->key_next - 1] = y;
    if (f->pointer_pinned) return;
    f->pointer_x = x, f->pointer_y = y;
}
static int32_t f_art(void *ctx, const char *name) { (void)ctx, (void)name; return 1; }
static const uint8_t *f_shape(void *ctx, int32_t bank, const char *tag)
{
    static uint8_t shape[16];
    (void)ctx, (void)bank, (void)tag;
    return shape;
}
static void f_zero_op(void *ctx, int32_t a, int32_t b) { (void)ctx, (void)a, (void)b; }
static void f_palette(void *ctx, uint8_t *rgb, int32_t first, int32_t count, const uint8_t *src, int32_t mode)
{
    (void)ctx, (void)rgb, (void)first, (void)count, (void)src, (void)mode;
}
static int32_t f_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    Fake *f = ctx;
    return f->pointer_x >= x0 && f->pointer_x < x1 && f->pointer_y >= y0 && f->pointer_y <= y1;
}
static void f_select(void *ctx, int32_t picture) { (void)ctx, (void)picture; }
static void f_blit(void *ctx, const uint8_t *shape) { (void)ctx, (void)shape; }
static int32_t f_picture(void *ctx, int32_t picture) { (void)ctx; return picture; }
static void f_fade(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)ctx, (void)first, (void)count, (void)rgb;
}
static int32_t f_buttons(void *ctx) { (void)ctx; return 0; }
static void f_void(void *ctx) { (void)ctx; }
static void f_screen(void *ctx, int32_t mode) { (void)ctx, (void)mode; }
static void f_music_start(void *ctx, const char *name) { (void)ctx, (void)name; }
static void f_free(void *ctx, int32_t bank) { (void)ctx, (void)bank; }
static void f_one(void *ctx, int32_t v) { (void)ctx, (void)v; }
static int32_t f_zero(void *ctx) { (void)ctx; return 0; }
static int32_t f_ticks(void *ctx) { (void)ctx; return 100; }
static void f_text(void *ctx, int32_t style, const char *text, int32_t x, int32_t y)
{
    Fake *f = ctx;
    (void)style;
    f->text_x = x, f->text_y = y;
    if (f->drawn_count < 128) snprintf(f->drawn[f->drawn_count++], sizeof f->drawn[0], "%s", text);
}
static int32_t f_text_width(void *ctx, const char *text) { (void)ctx; return (int32_t)strlen(text) * 8; }
static void f_panel(void *ctx, const uint8_t *rgb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t framed)
{
    (void)ctx, (void)rgb, (void)x0, (void)y0, (void)x1, (void)y1, (void)framed;
}

/* the EXTRAS screen itself: opened, then left on Done (it never writes here) */
static void f_extras(void *ctx)
{
    Fake *f = ctx;
    f->extras_opens++;
    f->selected_when_opened = f->drawn_count;
}

static int shown(const Fake *f, const char *text)
{
    for (int i = 0; i < f->drawn_count; i++)
        if (strcmp(f->drawn[i], text) == 0) return 1;
    return 0;
}

typedef struct Rig {
    Fake fake;
    int16_t transmission, automatic, abs, traction, opponent, player_car, net_car, laps, difficulty;
    int16_t race_type, track, music_volume, lost, pointer_active, buttons, net_off, quit, redraw_asked;
    int16_t link_game;
    int no_extras;   /* the original's ops: no extras op, no Extras row */
    int32_t screen, inverse;
} Rig;

static void setup(Rig *r)
{
    memset(r, 0, sizeof *r);
    r->transmission = 1;
    r->automatic = 0;
    r->track = 3;
    r->opponent = 9;
    r->player_car = 2;
    r->laps = 5;
    r->difficulty = 1;
    r->music_volume = 8;
    r->pointer_active = 1;
    r->screen = 0;
    r->inverse = 0;
    r->fake.pointer_x = 240;
    r->fake.pointer_y = 118;
}

static void run(Rig *r, const int *keys, int count)
{
    r->fake.keys = keys;
    r->fake.key_count = count;
    OptionsMenuState st = {
        &r->transmission, &r->automatic, &r->abs, &r->traction, &r->race_type, &r->track, &r->opponent,
        &r->player_car, &r->net_car, &r->laps, &r->difficulty, &r->music_volume, &r->lost, &r->pointer_active,
        &r->buttons, &r->screen, &r->inverse, &r->net_off, &r->quit, &r->redraw_asked };
    OptionsMenuOps ops = {
        .pointer_move_to = f_move, .load_art = f_art, .load_inverse = f_art, .music_feed = f_void,
        .shape = f_shape, .palette_set = f_palette, .text_styles = f_void, .pointer_poll = f_void,
        .pointer_mark = f_void, .pointer_in_rect = f_in_rect, .select = f_select, .blit_own = f_blit,
        .select_default = f_void, .pointer_hide = f_void, .lock = f_void, .picture_shape = f_picture,
        .unlock = f_void, .flip = f_void, .pointer_show = f_void, .pointer_buttons = f_buttons,
        .fade_in = f_fade, .deadline_set = f_one, .sound = f_one, .ticks = f_ticks,
        .pointer_activate_if_moved = f_void, .key = f_key, .pointer_click = f_void, .fade_out = f_zero_op,
        .free_inverse = f_void, .free = f_free, .audio = f_screen, .graphics = f_screen, .controls = f_screen,
        .load_save = f_screen, .music_fade_out = f_void, .music_end = f_void, .music_start = f_music_start,
        .credits = f_screen, .music_pick = f_void, .car_flags = f_void, .music_fade_to = f_one,
        .pointer_off = f_void, .music = f_void, .net_sync = f_void, .deadline_passed = f_zero,
        .extras = f_extras, .plus_text = f_text, .plus_text_width = f_text_width, .plus_panel = f_panel,
        .ctx = &r->fake };
    if (r->no_extras) ops.extras = NULL, ops.plus_text = NULL, ops.plus_text_width = NULL, ops.plus_panel = NULL;
    (void)options_menu(&st, &ops);
}

/* the row is drawn as text (no art tag), once Options is up */
static void test_extras_row_is_drawn_as_text(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK(shown(&r.fake, "Customize game"));
}

/* the row lines up with the art rows: their left edge (6) and their step (35 under Credits, measured on a capture) */
static void test_the_row_lines_up_with_the_others(void)
{
    Rig r;
    const int keys[] = { ESCAPE };
    setup(&r);
    run(&r, keys, 1);
    CHECK_EQ_INT(r.fake.text_x, 6);
    CHECK_EQ_INT(r.fake.text_y, 380);
}

/* with the row the pointer just under Credits' art is on it, and Credits keeps its art */
static void test_the_band_follows_the_row(void)
{
    Rig r;
    const int keys[] = { ENTER, ESCAPE };
    setup(&r);
    r.fake.pointer_pinned = 1;
    r.fake.pointer_y = 382;
    r.fake.pointer_x = 60;
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.extras_opens, 1);
}

/* Down from Credits lands on Extras, and one more on Done */
static void test_extras_is_the_last_element_before_done(void)
{
    Rig r;
    /* start on Transmission (row 0): 7 downs reach Credits, the 8th Extras, the 9th Done, then Done leaves */
    int keys[16];
    int n = 0;
    for (int i = 0; i < 9; i++) keys[n++] = DOWN;
    keys[n++] = ENTER;
    setup(&r);
    run(&r, keys, n);
    /* Done leaves without ever opening EXTRAS */
    CHECK_EQ_INT(r.fake.extras_opens, 0);
}

/* Enter on Extras opens the EXTRAS screen and keeps Options behind it */
static void test_enter_on_extras_opens_the_screen(void)
{
    Rig r;
    int keys[16];
    int n = 0;
    for (int i = 0; i < 8; i++) keys[n++] = DOWN;  /* to Extras */
    keys[n++] = ENTER;
    keys[n++] = ESCAPE;
    setup(&r);
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.extras_opens, 1);
}

/* the pointer in the Extras band selects it, and a click opens the screen */
static void test_the_pointer_reaches_the_extras_row(void)
{
    Rig r;
    const int keys[] = { ENTER, ESCAPE };
    setup(&r);
    r.fake.pointer_pinned = 1;
    r.fake.pointer_y = 410;  /* the band carved out of Done's top */
    r.fake.pointer_x = 60;
    run(&r, keys, 2);
    CHECK_EQ_INT(r.fake.extras_opens, 1);
}

/* without the extras op Options is the original's: no row, Down from Credits to Done, Up from Done to Credits */
static void test_without_the_op_no_extras_row(void)
{
    Rig r;
    int keys[16];
    int n = 0;
    for (int i = 0; i < 8; i++) keys[n++] = DOWN;   /* the 8th from Credits */
    keys[n++] = UP;
    setup(&r);
    r.no_extras = 1;
    run(&r, keys, n);
    CHECK_EQ_INT(r.fake.moved_y[6], 363);   /* Credits */
    CHECK_EQ_INT(r.fake.moved_y[7], 450);   /* Done */
    CHECK_EQ_INT(r.fake.moved_y[8], 363);   /* back on Credits */
    CHECK_EQ_INT(r.fake.extras_opens, 0);
}

/* without the op the pointer at y 410 is on Done's band (from 0x185), as in the original */
static void test_without_the_op_done_band_from_0x185(void)
{
    Rig r;
    const int keys[] = { ENTER };
    setup(&r);
    r.no_extras = 1;
    r.fake.pointer_pinned = 1;
    r.fake.pointer_y = 410;
    r.fake.pointer_x = 60;
    run(&r, keys, 1);
    CHECK_EQ_INT(r.fake.extras_opens, 0);
    CHECK_EQ_INT(r.fake.key_next, 1);   /* Done left on the Enter: no Escape asked after it */
}

int main(void)
{
    test_without_the_op_no_extras_row();
    test_without_the_op_done_band_from_0x185();
    test_extras_row_is_drawn_as_text();
    test_the_row_lines_up_with_the_others();
    test_the_band_follows_the_row();
    test_extras_is_the_last_element_before_done();
    test_enter_on_extras_opens_the_screen();
    test_the_pointer_reaches_the_extras_row();
    return TEST_REPORT();
}
