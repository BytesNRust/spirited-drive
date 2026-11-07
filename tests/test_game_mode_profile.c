/* Game Mode with the modes (game_mode.c): Profile, Single Player, Multiplayer, Exit, in the game's text; the cursor starts on Single Player, the arrows show a mode and apply it once they rest (a short fade only when the picture changes), the version at the foot. */
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
    int profile_net, profile_reads, profile_steps;   /* steps: profiles applied; net: where the cycle stands (3 profiles, round) */
    int peeks, last_peek, no_answer, clicks;           /* clicks: a bit per menu turn with the left button down */
    int drawn_profile;
    int exits_asked, apply_asked;
    int said_multiplayer, said_customize, said_single, said_exit;
    uint32_t clock, ms_per_call;   /* the fake clock: it advances with every ask for a key */
    int look, no_clock, fade_outs, fade_ins, said_return;   /* look: applying the mode changes the picture */
    int foot_big, foot_small, foot_low, foot_high_y, foot_right, foot_left, notice_y, version_y;   /* the foot lines as drawn: full size / small, below the row's top, ... */
} Fake;

static int32_t f_key(void *c)
{
    Fake *f = c;
    f->clock += f->ms_per_call;
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
    f->exits_asked += strcmp(t, "Exit to system ?") == 0;
    f->apply_asked += strcmp(t, "Exit to system ?") != 0;
    return 1;
}
static void f_free(void *c, int32_t b) { (void)c, (void)b; }
static void f_text(void *c, int32_t style, const char *t, int32_t x, int32_t y)
{
    Fake *f = c;
    (void)style;
    if (strncmp(t, "<  ", 3) == 0) f->drawn_profile++;
    f->said_multiplayer += strcmp(t, "Multiplayer") == 0, f->said_customize += strcmp(t, "Customize game") == 0;
    f->said_single += strcmp(t, "Single Player") == 0, f->said_exit += strcmp(t, "Exit") == 0;
    if (t[0] == 'v' || strncmp(t, "Update", 6) == 0) {
        f->foot_big++;
        if (y >= 0x191) f->foot_low++;
        (void)x;
    }
}
static void note_foot(Fake *f, const char *t, int32_t x, int32_t y, int32_t w);
static int32_t f_width(void *c, const char *t) { (void)c; return (int32_t)strlen(t) * 8; }
static void f_text_scaled(void *c, int32_t s, const char *t, int32_t x, int32_t y, int32_t n, int32_t d)
{
    (void)s;
    ((Fake *)c)->said_return += strstr(t, "Return") != NULL;
    if (t[0] == 'v' || strncmp(t, "Update", 6) == 0) note_foot(c, t, x, y, (int32_t)strlen(t) * 8 * n / d);
}
static void note_foot(Fake *f, const char *t, int32_t x, int32_t y, int32_t w)
{
    f->foot_small++;
    if (y >= 0x191) f->foot_low++;
    if (y > f->foot_high_y) f->foot_high_y = y;
    if (x + w > f->foot_right) f->foot_right = x + w;
    if (f->foot_left == 0 || x < f->foot_left) f->foot_left = x;
    if (t[0] == 'v') f->version_y = y; else f->notice_y = y;
}
static int32_t f_footer(void *c, int32_t line, char *text, size_t cap)
{
    (void)c;
    if (line > 1) return 0;
    snprintf(text, cap, "%s", line == 0 ? "v0.9.0-dev+abcdef0" : "Update available: v9.9.9");
    return 1;
}
static void f_open(void *c) { (void)c; }
static int32_t f_width_scaled(void *c, const char *t, int32_t n, int32_t d) { (void)c; return (int32_t)strlen(t) * 8 * n / d; }
static void f_cover(void *c, int32_t x, int32_t y, int32_t w, int32_t h) { (void)c, (void)x, (void)y, (void)w, (void)h; }
static int mod3(int n) { return ((n % 3) + 3) % 3; }
static int32_t f_profile(void *c, int32_t step, const char *text[3])
{
    Fake *f = c;
    f->profile_reads++;
    text[0] = "Stock '96", text[1] = "line one", text[2] = "line two";
    if (step >= GAME_MODE_PROFILE_LOOK - 3 && step <= GAME_MODE_PROFILE_LOOK + 3) return f->look;
    if (step >= GAME_MODE_PROFILE_APPLY - 3 && step <= GAME_MODE_PROFILE_APPLY + 3) {
        f->profile_steps++, f->profile_net += step - GAME_MODE_PROFILE_APPLY;
        return 1;
    }
    if (step >= GAME_MODE_PROFILE_PEEK - 3 && step <= GAME_MODE_PROFILE_PEEK + 3) {
        f->peeks++, f->last_peek = step - GAME_MODE_PROFILE_PEEK;
        return mod3(f->last_peek) != 0 ? 2 : 1;
    }
    return 1;
}
static uint32_t f_clock(void *c) { return ((Fake *)c)->clock; }
static void f_quick_out(void *c) { ((Fake *)c)->fade_outs++; }
static void f_quick_in(void *c, const uint8_t *rgb) { (void)rgb; ((Fake *)c)->fade_ins++; }
static int32_t f_buttons(void *c)
{
    const Fake *f = c;
    const int turn = (f->key_calls - 1) / 2;   /* read between the turn's two asks for a key */
    return turn >= 0 && turn < 31 && (f->clicks >> turn & 1) ? 1 : 0;
}

static int32_t run_with(Fake *f, const int *keys, int count, int with_profile, int with_footer)
{
    f->keys = keys, f->key_count = count;
    GameModeState st = { 0 };
    char name[32] = "";
    st.name = name;
    st.pointer_active = 1;
    GameModeOps ops = {
        .palette_clear = f_clear, .pointer_move_to = f_move, .sound_load = f_void, .load_art = f_art, .music_feed = f_void, .shape = f_shape, .palette_set = f_pal,
        .pointer_mark = f_void, .pointer_in_rect = f_in_rect, .select = f_int, .blit_own = f_blit, .select_default = f_void, .pointer_hide = f_void, .lock = f_void,
        .picture_shape = f_picture, .unlock = f_void, .flip = f_void, .pointer_show = f_void, .pointer_buttons = f_buttons, .fade_in = f_fade_in, .inverse_build = f_void,
        .banner_wait = f_banner, .deadline_set = f_int, .sound = f_int, .key = f_key, .ticks = f_ticks, .pointer_activate_if_moved = f_void, .pointer_click = f_void,
        .name_box = f_name, .fade_out = f_fade_out, .free_inverse = f_void, .modem = f_zero, .network_found = f_zero, .network_game = f_zero, .yes_no = f_yes,
        .drain = f_void, .pointer_off = f_void, .music = f_void, .deadline_passed = f_zero, .sound_free = f_void, .free = f_free, .ctx = f,
        .plus_text = f_text, .plus_text_width = f_width, .extras_cover = f_cover };
    if (with_profile) ops.profile = f_profile, ops.plus_text_scaled = f_text_scaled, ops.plus_text_width_scaled = f_width_scaled;
    if (with_profile && !f->no_clock) ops.clock_ms = f_clock, ops.fade_out_quick = f_quick_out, ops.fade_in_quick = f_quick_in;
    if (with_footer) ops.footer = f_footer, ops.open_notice = f_open;
    return game_mode_menu(0, &st, &ops);
}
static int32_t run(Fake *f, const int *keys, int count, int with_profile) { return run_with(f, keys, count, with_profile, 0); }

static void rig(Fake *f, int x, int y, int pinned)
{
    memset(f, 0, sizeof *f);
    f->pointer_x = x, f->pointer_y = y, f->pinned = pinned;
    f->ms_per_call = 25;   /* a key and the turn after it with none are 100 ms */
}

enum { ROW_PROFILE = 0x91, ROW_SINGLE = 0xda, ROW_MULTI = 0x122, ROW_EXIT = 0x16f };

/* Single Player first; Down runs Multiplayer, Exit, Profile and round; Up from it is the Profile row */
static void test_the_order(void)
{
    Fake f;
    int keys[8], n = 0;
    for (int i = 0; i < 4; i++) keys[n++] = DOWN;
    rig(&f, 320, ROW_SINGLE, 0);
    run(&f, keys, n, 1);
    CHECK_EQ_INT(f.moved_y[0], ROW_SINGLE);
    CHECK_EQ_INT(f.moved_y[1], ROW_MULTI);
    CHECK_EQ_INT(f.moved_y[2], ROW_EXIT);
    CHECK_EQ_INT(f.moved_y[3], ROW_PROFILE);
    CHECK_EQ_INT(f.moved_y[4], ROW_SINGLE);
    const int up[] = { UP };
    rig(&f, 320, ROW_SINGLE, 0);
    run(&f, up, 1, 1);
    CHECK_EQ_INT(f.moved_y[1], ROW_PROFILE);
}

static void test_the_words(void)
{
    Fake f;
    const int none[] = { ESCAPE };
    rig(&f, 320, ROW_SINGLE, 0);
    run(&f, none, 1, 1);
    CHECK(f.drawn_profile >= 1);
    CHECK(f.said_single >= 1 && f.said_multiplayer >= 1 && f.said_exit >= 1);
    CHECK_EQ_INT(f.said_customize, 0);   /* Customize is Options' now */
}

/* rapid arrows (+1 +1 -1: one along) change the row at once and apply the mode once, after they rest 400 ms: nothing is lost */
static void test_the_arrows_apply_once_they_rest(void)
{
    Fake f;
    const int keys[] = { RIGHT, RIGHT, LEFT, 0, 0, 0, 0, 0, 0, 0, 0 };
    rig(&f, 320, ROW_PROFILE, 1);
    run(&f, keys, 11, 1);
    CHECK_EQ_INT(f.profile_steps, 1);
    CHECK_EQ_INT(f.profile_net, 1);
    CHECK_EQ_INT(f.apply_asked, 0);
    CHECK(f.peeks >= 3);   /* every press showed its mode at once */
}

/* a mode is not applied before the arrows have rested */
static void test_it_waits_for_the_arrows_to_rest(void)
{
    Fake f;
    const int keys[] = { RIGHT, 0 };
    rig(&f, 320, ROW_PROFILE, 1);
    run(&f, keys, 2, 1);
    CHECK_EQ_INT(f.profile_steps, 0);
    CHECK_EQ_INT(f.said_return, 0);   /* and the row no longer says Return applies it */
}

/* with no clock the arrow applies at once */
static void test_no_clock_applies_at_once(void)
{
    Fake f;
    const int keys[] = { RIGHT, ENTER };
    rig(&f, 320, ROW_PROFILE, 1);
    f.no_clock = 1;
    run(&f, keys, 2, 1);
    CHECK_EQ_INT(f.profile_steps, 1);
    CHECK_EQ_INT(f.profile_net, 1);
}

/* three steps come round to the one in use: nothing applies */
static void test_round_applies_nothing(void)
{
    Fake f;
    const int keys[] = { RIGHT, RIGHT, RIGHT, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    rig(&f, 320, ROW_PROFILE, 1);
    run(&f, keys, 12, 1);
    CHECK_EQ_INT(f.profile_steps, 0);
    const int back[] = { LEFT, LEFT, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    rig(&f, 320, ROW_PROFILE, 1);
    run(&f, back, 11, 1);
    CHECK_EQ_INT(f.profile_net, -2);
}

/* leaving the row applies what it shows */
static void test_leaving_the_row_applies(void)
{
    Fake f;
    const int keys[] = { UP, RIGHT, DOWN, UP, ENTER };
    rig(&f, 320, ROW_SINGLE, 0);
    run(&f, keys, 5, 1);
    CHECK_EQ_INT(f.profile_steps, 1);
}

/* the short fade, out and back in, only when the mode changes the image's format or resolution */
static void test_the_fade_only_when_the_picture_changes(void)
{
    Fake f;
    const int keys[] = { RIGHT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    rig(&f, 320, ROW_PROFILE, 1);
    f.look = 1;
    run(&f, keys, 12, 1);
    CHECK_EQ_INT(f.profile_steps, 1);
    CHECK_EQ_INT(f.fade_outs, 1);
    CHECK_EQ_INT(f.fade_ins, 1);
    rig(&f, 320, ROW_PROFILE, 1);
    f.look = 0;
    run(&f, keys, 12, 1);
    CHECK_EQ_INT(f.profile_steps, 1);
    CHECK_EQ_INT(f.fade_outs, 0);
    CHECK_EQ_INT(f.fade_ins, 0);
}

/* a click on the row's left third shows the previous, on the right third the next, and both apply once they rest; on the name it applies what is shown */
static void test_clicks_show_and_the_name_applies(void)
{
    Fake f;
    const int idle[] = { 0, 0, 0, 0 };
    rig(&f, 60, ROW_PROFILE, 1);
    f.clicks = 1 << 3;
    run(&f, idle, 4, 1);
    CHECK_EQ_INT(f.last_peek, -1);
    CHECK_EQ_INT(f.profile_steps, 0);   /* not yet: the arrows have not rested */
    rig(&f, 600, ROW_PROFILE, 1);
    f.clicks = 1 << 3;
    run(&f, idle, 4, 1);
    CHECK_EQ_INT(f.last_peek, 1);
    CHECK_EQ_INT(f.profile_steps, 0);
    const int right[] = { RIGHT, 0, 0, 0 };
    rig(&f, 320, ROW_PROFILE, 1);
    f.clicks = 1 << 5;   /* the click on the name: the mode shown applies without waiting */
    run(&f, right, 4, 1);
    CHECK_EQ_INT(f.profile_steps, 1);
    CHECK_EQ_INT(f.profile_net, 1);
}

/* the bands are the art's four: Exit's from 0x14a to the screen's bottom leaves on the Return */
static void test_the_pointer_bands(void)
{
    Fake f;
    const int enter[] = { ENTER };
    rig(&f, 320, 0x14a, 1);
    CHECK_EQ_INT(run(&f, enter, 1, 1), 1);
    CHECK_EQ_INT(f.key_calls, 2);
    rig(&f, 320, 0x149, 1);   /* Multiplayer's, with no network found: not Exit */
    CHECK_EQ_INT(run(&f, enter, 1, 1), 1);
    CHECK(f.key_calls > 2);
}

/* the version and the update notice: at the foot in the small subtitle size, with a margin from the left edge and the bottom, the notice above the version */
static void test_the_foot_lines(void)
{
    Fake f;
    const int none[] = { ESCAPE };
    rig(&f, 320, ROW_SINGLE, 0);
    run_with(&f, none, 1, 1, 1);
    CHECK_EQ_INT(f.foot_big, 0);   /* not the big letters */
    CHECK(f.foot_small >= 2);
    CHECK(f.foot_low >= 2);
    CHECK(f.foot_left >= 12);   /* a margin from the left edge */
    CHECK(f.version_y + 20 <= 480 - 10);   /* and from the bottom, the small line being about 17 high */
    CHECK(f.notice_y < f.version_y);
    CHECK(f.version_y - f.notice_y >= 17);   /* the lines do not touch */
}

int main(void)
{
    test_the_order();
    test_the_words();
    test_the_arrows_apply_once_they_rest();
    test_it_waits_for_the_arrows_to_rest();
    test_no_clock_applies_at_once();
    test_round_applies_nothing();
    test_leaving_the_row_applies();
    test_the_fade_only_when_the_picture_changes();
    test_clicks_show_and_the_name_applies();
    test_the_pointer_bands();
    test_the_foot_lines();
    return TEST_REPORT();
}
