/* Game Mode's Profile row (game_mode.c, the port's fifth item under Exit): selected with Up / Down, stepped with the arrows, Return and the pointer, its band taking the foot of
 * Exit's; without the profile op the menu is the original's four items. */
#include "game/frontend/game_mode.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000 };

typedef struct Fake {
    const int *keys;
    int key_count, key_calls;   /* the menu asks twice a turn: one scripted key serves both asks of its turn */
    int pointer_x, pointer_y, pinned;
    int moved_y[16], moved_count;
    int profile_net, profile_reads, profile_steps;
    int drawn_profile;
    int exits_asked;
} Fake;

static int32_t f_key(void *c)
{
    Fake *f = c;
    const int turn = f->key_calls++ / 2;   /* a key, then a turn with none (the menu takes a key only once the last one is let go) */
    if (turn % 2 != 0) return 0;
    return turn / 2 < f->key_count ? f->keys[turn / 2] : ESCAPE;
}
static void f_move(void *c, int32_t x, int32_t y)
{
    Fake *f = c;
    if (f->moved_count < 16) f->moved_y[f->moved_count++] = y;
    if (!f->pinned) f->pointer_x = x, f->pointer_y = y;
}
static int32_t f_in_rect(void *c, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    const Fake *f = c;
    return f->pointer_x >= x0 && f->pointer_x < x1 && f->pointer_y >= y0 && f->pointer_y <= y1;
}
static int32_t f_art(void *c, const char *n) { (void)c, (void)n; return 1; }
static const uint8_t *f_shape(void *c, int32_t b, const char *t)
{
    static uint8_t shape[0x20];
    (void)c, (void)b, (void)t;
    return shape;
}
static void f_void(void *c) { (void)c; }
static void f_clear(void *c, uint8_t *rgb) { (void)c, (void)rgb; }
static void f_pal(void *c, uint8_t *rgb, int32_t a, int32_t n, const uint8_t *s, int32_t m) { (void)c, (void)rgb, (void)a, (void)n, (void)s, (void)m; }
static void f_int(void *c, int32_t v) { (void)c, (void)v; }
static void f_blit(void *c, const uint8_t *s) { (void)c, (void)s; }
static int32_t f_picture(void *c, int32_t p) { (void)c; return p; }
static int32_t f_zero(void *c) { (void)c; return 0; }
static int32_t f_ticks(void *c) { (void)c; return 0x20; }
static void f_fade_in(void *c, int32_t a, int32_t n, const uint8_t *r) { (void)c, (void)a, (void)n, (void)r; }
static void f_banner(void *c, const char *t, int32_t n) { (void)c, (void)t, (void)n; }
static int32_t f_name(void *c, char *n) { (void)c, (void)n; return 1; }
static void f_fade_out(void *c, int32_t a, int32_t n) { (void)c, (void)a, (void)n; }
static int32_t f_yes(void *c, const char *t)
{
    Fake *f = c;
    (void)t;
    f->exits_asked++;
    return 1;
}
static void f_free(void *c, int32_t b) { (void)c, (void)b; }
static void f_text(void *c, int32_t style, const char *t, int32_t x, int32_t y)
{
    Fake *f = c;
    (void)style, (void)x, (void)y;
    if (strncmp(t, "<  ", 3) == 0) f->drawn_profile++;
}
static int32_t f_width(void *c, const char *t) { (void)c; return (int32_t)strlen(t) * 8; }
static void f_text_scaled(void *c, int32_t s, const char *t, int32_t x, int32_t y, int32_t n, int32_t d) { (void)c, (void)s, (void)t, (void)x, (void)y, (void)n, (void)d; }
static int32_t f_width_scaled(void *c, const char *t, int32_t n, int32_t d) { (void)c; return (int32_t)strlen(t) * 8 * n / d; }
static void f_cover(void *c, int32_t x, int32_t y, int32_t w, int32_t h) { (void)c, (void)x, (void)y, (void)w, (void)h; }
static int32_t f_profile(void *c, int32_t step, const char *text[3])
{
    Fake *f = c;
    f->profile_reads++;
    if (step != 0) f->profile_steps++;
    f->profile_net += step;
    text[0] = "Stock '96", text[1] = "line one", text[2] = "line two";
    return 1;
}

static int32_t run(Fake *f, const int *keys, int count, int with_profile)
{
    f->keys = keys, f->key_count = count;
    GameModeState st = { 0 };
    char name[32] = "";
    st.name = name;
    st.pointer_active = 1;
    GameModeOps ops = {
        .palette_clear = f_clear, .pointer_move_to = f_move, .sound_load = f_void, .load_art = f_art, .music_feed = f_void, .shape = f_shape, .palette_set = f_pal,
        .pointer_mark = f_void, .pointer_in_rect = f_in_rect, .select = f_int, .blit_own = f_blit, .select_default = f_void, .pointer_hide = f_void, .lock = f_void,
        .picture_shape = f_picture, .unlock = f_void, .flip = f_void, .pointer_show = f_void, .pointer_buttons = f_zero, .fade_in = f_fade_in, .inverse_build = f_void,
        .banner_wait = f_banner, .deadline_set = f_int, .sound = f_int, .key = f_key, .ticks = f_ticks, .pointer_activate_if_moved = f_void, .pointer_click = f_void,
        .name_box = f_name, .fade_out = f_fade_out, .free_inverse = f_void, .modem = f_zero, .network_found = f_zero, .network_game = f_zero, .yes_no = f_yes,
        .drain = f_void, .pointer_off = f_void, .music = f_void, .deadline_passed = f_zero, .sound_free = f_void, .free = f_free, .ctx = f,
        .plus_text = f_text, .plus_text_width = f_width, .extras_cover = f_cover };
    if (with_profile) ops.profile = f_profile, ops.plus_text_scaled = f_text_scaled, ops.plus_text_width_scaled = f_width_scaled;
    return game_mode_menu(0, &st, &ops);
}

static void rig(Fake *f, int x, int y, int pinned)
{
    memset(f, 0, sizeof *f);
    f->pointer_x = x, f->pointer_y = y, f->pinned = pinned;
}

/* without the op: four items, Down from Exit wraps to Single player, the row never drawn or asked */
static void test_without_the_op_four_items(void)
{
    Fake f;
    int keys[8], n = 0;
    for (int i = 0; i < 4; i++) keys[n++] = DOWN;
    rig(&f, 320, 0x91, 0);
    run(&f, keys, n, 0);
    CHECK_EQ_INT(f.moved_y[4], 0x91);   /* the 4th Down: back on Single player */
    CHECK_EQ_INT(f.profile_reads, 0);
    CHECK_EQ_INT(f.drawn_profile, 0);
}

/* with it Down from Exit lands on the row (pointer y 0x1c0), one more wraps to Single player; Up from Single player lands on the row */
static void test_the_row_is_the_fifth_item(void)
{
    Fake f;
    int keys[8], n = 0;
    for (int i = 0; i < 5; i++) keys[n++] = DOWN;
    rig(&f, 320, 0x91, 0);
    run(&f, keys, n, 1);
    CHECK_EQ_INT(f.moved_y[4], 0x1c0);
    CHECK_EQ_INT(f.moved_y[5], 0x91);
    CHECK(f.drawn_profile >= 1);
    const int up[] = { UP };
    rig(&f, 320, 0x91, 0);
    run(&f, up, 1, 1);
    CHECK_EQ_INT(f.moved_y[1], 0x1c0);
}

/* the arrows and Return on the row step it: +1 +1 -1 +1 */
static void test_the_arrows_and_return_step_the_profile(void)
{
    Fake f;
    int keys[12], n = 0;
    for (int i = 0; i < 4; i++) keys[n++] = DOWN;
    keys[n++] = RIGHT, keys[n++] = RIGHT, keys[n++] = LEFT, keys[n++] = ENTER;
    rig(&f, 320, 0x91, 0);
    run(&f, keys, n, 1);
    CHECK_EQ_INT(f.profile_steps, 4);
    CHECK_EQ_INT(f.profile_net, 2);
}

/* the bands: Exit's ends at 0x190 and the row's takes 0x191..0x1e0; a click on the row's left third steps back, elsewhere forward */
static void test_the_pointer_bands(void)
{
    Fake f;
    const int enter[] = { ENTER };
    rig(&f, 320, 0x190, 1);
    CHECK_EQ_INT(run(&f, enter, 1, 1), 1);   /* Exit: leaves on the Return */
    CHECK_EQ_INT(f.profile_steps, 0);
    CHECK_EQ_INT(f.key_calls, 2);
    rig(&f, 400, 0x191, 1);
    run(&f, enter, 1, 1);
    CHECK_EQ_INT(f.profile_net, 1);
    rig(&f, 60, 0x1c0, 1);
    run(&f, enter, 1, 1);
    CHECK_EQ_INT(f.profile_net, -1);
    rig(&f, 320, 0x191, 1);   /* without the op the same spot is Exit's */
    CHECK_EQ_INT(run(&f, enter, 1, 0), 1);
    CHECK_EQ_INT(f.key_calls, 2);
}

int main(void)
{
    test_without_the_op_four_items();
    test_the_row_is_the_fifth_item();
    test_the_arrows_and_return_step_the_profile();
    test_the_pointer_bands();
    return TEST_REPORT();
}
