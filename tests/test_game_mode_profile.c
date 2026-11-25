/* Game Mode, L2b (game_mode.c; notes/dep59-step4-editions-plan.md, section 6; reordered by the owner 2026-11-17): the selector of the
 * modes and editions on top with its lines, then Single Player, Multiplayer and "Manage content", Exit at the bottom right, the version and the update notice at the
 * bottom left. The cursor starts on Single Player; the selector's arrows show a choice and apply it once they rest (a short fade only when
 * the picture changes); a choice that cannot be used is shown and never applied. */
#include "game/frontend/game_mode.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

enum { ENTER = 0xd, ESCAPE = 0x1b, UP = 0x4800, LEFT = 0x4b00, RIGHT = 0x4d00, DOWN = 0x5000 };
enum { STYLE_DIM = 4 };

typedef struct Rect { int x, y, w, set; } Rect;   /* where a text was last drawn */

typedef struct Fake {
    const int *keys;
    int key_count, key_calls;   /* the menu asks twice a turn: one scripted key serves both asks of its turn */
    int pointer_x, pointer_y, pinned;
    int moved_y[16], moved_count;
    int count, in_use, unusable, look;            /* the choices: how many, the one in use, one that cannot be used (-1 none) */
    int peeks, last_peek, applies, last_apply;    /* the choice op's calls */
    int no_clock, fade_outs, fade_ins;
    uint32_t clock, ms_per_call;                  /* the fake clock: it advances with every ask for a key */
    int clicks;                                   /* a bit per menu turn with the left button down */
    int exits_asked, names_asked, networks_asked, banners, manage_banner;
    const char *network_why;                      /* dep59 step 4: the reason Multiplayer is closed (an edition in use), NULL open */
    int network_banner, multi_style;
    int timer_after_banner;                       /* the idle deadline set again after the last banner (0 when a banner came last) */
    int32_t result;                               /* game_mode_menu's, from run */
    int manages, manage_shown, manage_answer;     /* T7: the CONTENT screen opened, on which choice, and the one it closed on */
    int said_customize;
    char last_title[64];
    Rect single, multi, title, manage, exit, version, notice;
    int manage_style, exit_style, notice_hot, notice_plain, notice_last, version_hot, opens;
    int move_at, move_x, move_y;                  /* at that ask for a key (when not 0) the pointer moves there */
} Fake;

static int32_t f_key(void *c)
{
    Fake *f = c;
    f->clock += f->ms_per_call;
    if (f->move_at != 0 && f->key_calls == f->move_at) f->pointer_x = f->move_x, f->pointer_y = f->move_y;
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
static int32_t f_network_found(void *c) { ((Fake *)c)->networks_asked++; return 0; }
static int32_t f_ticks(void *c) { (void)c; return 0x20; }
static void f_fade_in(void *c, int32_t a, int32_t n, const uint8_t *r) { (void)c, (void)a, (void)n, (void)r; }
static void f_deadline(void *c, int32_t v) { (void)v, ((Fake *)c)->timer_after_banner = 1; }
/* the one deadline the screen and its banners share: a banner waits it out, so it has passed until the screen sets it again */
static int32_t f_deadline_passed(void *c) { const Fake *f = c; return f->banners > 0 && !f->timer_after_banner; }
static void f_banner(void *c, const char *t, int32_t n)
{
    Fake *f = c;
    (void)n;
    f->banners++;
    f->timer_after_banner = 0;
    f->manage_banner += strcmp(t, "Editions are managed in a later version") == 0;
    f->network_banner += f->network_why != NULL && strcmp(t, f->network_why) == 0;
}
static const char *f_network_closed(void *c) { return ((Fake *)c)->network_why; }
static int32_t f_manage(void *c, int32_t shown)
{
    Fake *f = c;
    f->manages++, f->manage_shown = shown;
    return f->manage_answer;
}
static int32_t f_name(void *c, char *n) { (void)n; ((Fake *)c)->names_asked++; return 1; }
static void f_fade_out(void *c, int32_t a, int32_t n) { (void)c, (void)a, (void)n; }
static int32_t f_yes(void *c, const char *t)
{
    ((Fake *)c)->exits_asked += strcmp(t, "Exit to system ?") == 0;
    return 1;
}
static void f_free(void *c, int32_t b) { (void)c, (void)b; }
static int32_t f_width(void *c, const char *t) { (void)c; return (int32_t)strlen(t) * 16; }
static int32_t f_width_scaled(void *c, const char *t, int32_t n, int32_t d) { (void)c; return (int32_t)strlen(t) * 16 * n / d; }
static void note(Rect *r, int x, int y, int w) { r->x = x, r->y = y, r->w = w, r->set = 1; }
static void f_text(void *c, int32_t style, const char *t, int32_t x, int32_t y)
{
    Fake *f = c;
    const int w = (int)strlen(t) * 16;
    if (strncmp(t, "<  ", 3) == 0) note(&f->title, x, y, w), snprintf(f->last_title, sizeof f->last_title, "%s", t);
    else if (strcmp(t, "Single Player") == 0) note(&f->single, x, y, w);
    else if (strcmp(t, "Multiplayer") == 0) note(&f->multi, x, y, w), f->multi_style = style;
    else if (strcmp(t, "Manage content") == 0) note(&f->manage, x, y, w), f->manage_style = style;
    else if (strcmp(t, "Exit") == 0) note(&f->exit, x, y, w), f->exit_style = style;
    f->said_customize += strcmp(t, "Customize game") == 0;
}
static void f_text_scaled(void *c, int32_t s, const char *t, int32_t x, int32_t y, int32_t n, int32_t d)
{
    Fake *f = c;
    const int w = (int)strlen(t) * 16 * n / d;
    if (t[0] == 'v') note(&f->version, x, y, w), f->version_hot += s != 0;
    else if (strncmp(t, "Update", 6) == 0) note(&f->notice, x, y, w), f->notice_hot += s != 0, f->notice_plain += s == 0, f->notice_last = s;
}
static int32_t f_footer(void *c, int32_t line, char *text, size_t cap)
{
    (void)c;
    if (line > 1) return 0;
    snprintf(text, cap, "%s", line == 0 ? "v0.9.0-dev+abcdef0" : "Update available: v9.9.9");
    return 1;
}
static void f_open(void *c) { ((Fake *)c)->opens++; }
static void f_cover(void *c, int32_t x, int32_t y, int32_t w, int32_t h) { (void)c, (void)x, (void)y, (void)w, (void)h; }

static const char *const NAMES[] = { "Stock '96", "Restored '96", "Restomod '26", "Europa 80", "Moon", "Rally" };
static int32_t f_choice(void *c, int32_t op, int32_t index, const char *text[3])
{
    Fake *f = c;
    if (op == GAME_MODE_CHOICE_COUNT) return f->count;
    if (op == GAME_MODE_CHOICE_READ) index = f->in_use;
    if (index < 0 || index >= f->count) return -1;
    text[0] = NAMES[index], text[1] = index < 3 ? "line one" : "A Restomod '26 edition", text[2] = index < 3 ? "line two" : "";
    switch (op) {
    case GAME_MODE_CHOICE_READ: return f->in_use;
    case GAME_MODE_CHOICE_PEEK: f->peeks++, f->last_peek = index; return index != f->unusable;
    case GAME_MODE_CHOICE_LOOK: return f->look;
    case GAME_MODE_CHOICE_APPLY:
        if (index == f->unusable) return 0;
        f->applies++, f->last_apply = index, f->in_use = index;
        return 1;
    }
    return -1;
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

enum { GAME_MODE_RESULT_LEAVE = 1 };   /* game_mode.c's LEAVE: Escape's Yes */

static int32_t run(Fake *f, const int *keys, int count)
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
        .banner_wait = f_banner, .deadline_set = f_deadline, .sound = f_int, .key = f_key, .ticks = f_ticks, .pointer_activate_if_moved = f_void, .pointer_click = f_void,
        .name_box = f_name, .fade_out = f_fade_out, .free_inverse = f_void, .modem = f_zero, .network_found = f_network_found, .network_game = f_zero, .yes_no = f_yes,
        .drain = f_void, .pointer_off = f_void, .music = f_void, .deadline_passed = f_deadline_passed, .sound_free = f_void, .free = f_free, .ctx = f,
        .plus_text = f_text, .plus_text_width = f_width, .extras_cover = f_cover, .choice = f_choice, .plus_text_scaled = f_text_scaled,
        .plus_text_width_scaled = f_width_scaled, .footer = f_footer, .open_notice = f_open, .network_closed = f_network_closed };
    if (!f->no_clock) ops.clock_ms = f_clock, ops.fade_out_quick = f_quick_out, ops.fade_in_quick = f_quick_in;
    if (f->manage_answer != 0) ops.manage = f_manage;   /* T7: with a CONTENT screen (0 in a rig: the banner of before) */
    return f->result = game_mode_menu(0, &st, &ops);
}

static void rig(Fake *f, int x, int y, int pinned)
{
    memset(f, 0, sizeof *f);
    f->pointer_x = x, f->pointer_y = y, f->pinned = pinned;
    f->ms_per_call = 25;   /* a key and the turn after it with none are 100 ms */
    f->count = 5, f->in_use = 2, f->unusable = -1;   /* the three modes and two editions; Restomod '26 in use */
}

/* the rows' centres (the owner's reorder: the selector on top, the three options 50 apart under it, Exit at the foot; GAP is between
 * Manage content and Exit, on no row) */
enum { ROW_CHOICE = 140, ROW_SINGLE = 215, ROW_MULTI = 265, ROW_EXIT = 315, EXIT_X = 320, GAP = 370 };

/* Single Player first; Down runs Multiplayer, Exit, the selector and round (Manage content is hidden: the owner's 2026-11-25); Up from Single Player is the selector */
static void test_the_order(void)
{
    Fake f;
    int keys[8], n = 0;
    for (int i = 0; i < 4; i++) keys[n++] = DOWN;
    rig(&f, 320, ROW_SINGLE, 0);
    run(&f, keys, n);
    CHECK_EQ_INT(f.moved_y[0], ROW_SINGLE);
    CHECK_EQ_INT(f.moved_y[1], ROW_MULTI);
    CHECK_EQ_INT(f.moved_y[2], ROW_EXIT);
    CHECK_EQ_INT(f.moved_y[3], ROW_CHOICE);
    CHECK_EQ_INT(f.moved_y[4], ROW_SINGLE);
    const int up[] = { UP };
    rig(&f, 320, ROW_SINGLE, 0);
    run(&f, up, 1);
    CHECK_EQ_INT(f.moved_y[1], ROW_CHOICE);
}

/* the selector on top with its lines, then Single Player, Multiplayer and Exit 50 apart, in one size and centred (no Manage content row), the version at
 * the bottom left */
static void test_the_layout(void)
{
    Fake f;
    const int none[] = { ESCAPE };
    rig(&f, 320, ROW_SINGLE, 0);
    run(&f, none, 1);
    CHECK(f.single.set && f.multi.set && f.title.set && f.exit.set && f.version.set && f.notice.set);
    CHECK(f.title.y < f.single.y && f.single.y < f.multi.y && f.multi.y < f.exit.y);
    CHECK(f.title.y > 88);   /* under the title band */
    /* the three options are one size (a big word's centre is 21 pixels below where it is drawn), 50 apart: text centres 215, 265, 315 */
    CHECK_EQ_INT(f.single.y + 21, 215);
    CHECK_EQ_INT(f.multi.y + 21, 265);
    CHECK_EQ_INT(f.exit.y + 21, 315);
    CHECK_EQ_INT(f.exit.x + f.exit.w / 2, 320);   /* centred like the two over it */
    CHECK_EQ_INT(f.manage.set, 0);   /* Manage content is not drawn in any mode */
    CHECK(f.exit.y + 42 < f.version.y);
    CHECK_EQ_INT(f.version.x, 14);
    CHECK_EQ_INT(f.said_customize, 0);
    CHECK(strcmp(f.last_title, "<  Restomod '26  >") == 0);
}

/* rapid arrows (+1 +1 -1: one along) change the row at once and apply the choice once, after they rest 400 ms */
static void test_the_arrows_apply_once_they_rest(void)
{
    Fake f;
    const int keys[] = { RIGHT, RIGHT, LEFT, 0, 0, 0, 0, 0, 0, 0, 0 };
    rig(&f, 320, ROW_CHOICE, 1);
    run(&f, keys, 11);
    CHECK_EQ_INT(f.applies, 1);
    CHECK_EQ_INT(f.last_apply, 3);
    CHECK(f.peeks >= 3);   /* every press showed its choice at once */
}

/* a choice is not applied before the arrows have rested; with no clock at once */
static void test_it_waits_for_the_arrows_to_rest(void)
{
    Fake f;
    const int keys[] = { RIGHT, 0 };
    rig(&f, 320, ROW_CHOICE, 1);
    run(&f, keys, 2);
    CHECK_EQ_INT(f.applies, 0);
    const int now[] = { RIGHT, ENTER };
    rig(&f, 320, ROW_CHOICE, 1);
    f.no_clock = 1;
    run(&f, now, 2);
    CHECK_EQ_INT(f.applies, 1);
    CHECK_EQ_INT(f.last_apply, 3);
}

/* the selector goes round the choices by index: from the last to the first and back; all the way round applies nothing */
static void test_the_selector_goes_round(void)
{
    Fake f;
    const int keys[] = { RIGHT, RIGHT, RIGHT, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    rig(&f, 320, ROW_CHOICE, 1);   /* 2 -> 3 -> 4 -> 0 */
    run(&f, keys, 12);
    CHECK_EQ_INT(f.last_apply, 0);
    const int back[] = { LEFT, LEFT, LEFT, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    rig(&f, 320, ROW_CHOICE, 1);   /* 2 -> 1 -> 0 -> 4 */
    run(&f, back, 12);
    CHECK_EQ_INT(f.last_apply, 4);
    const int round[] = { RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, 0, 0, 0, 0, 0, 0, 0, 0 };
    rig(&f, 320, ROW_CHOICE, 1);
    run(&f, round, 13);
    CHECK_EQ_INT(f.applies, 0);
}

/* a choice that cannot be used is shown (its line the reason) but never applied, by resting or by Return; leaving the row shows the one in
 * use again */
static void test_a_choice_that_cannot_be_used(void)
{
    Fake f;
    const int rest[] = { RIGHT, 0, 0, 0, 0, 0, 0, 0, 0, ENTER, 0, 0 };
    rig(&f, 320, ROW_CHOICE, 1);
    f.unusable = 3;
    run(&f, rest, 12);
    CHECK_EQ_INT(f.applies, 0);
    CHECK(strcmp(f.last_title, "<  Europa 80  >") == 0);
    const int leave[] = { RIGHT, 0, DOWN, 0, 0 };
    rig(&f, 320, ROW_CHOICE, 0);
    f.unusable = 3;
    run(&f, leave, 5);
    CHECK_EQ_INT(f.applies, 0);
    CHECK(strcmp(f.last_title, "<  Restomod '26  >") == 0);
}

/* leaving the row applies what it shows */
static void test_leaving_the_row_applies(void)
{
    Fake f;
    const int keys[] = { UP, RIGHT, DOWN, ESCAPE };
    rig(&f, 320, ROW_SINGLE, 0);
    run(&f, keys, 4);
    CHECK_EQ_INT(f.applies, 1);
    CHECK_EQ_INT(f.last_apply, 3);
}

/* the short fade, out and back in, only when the choice changes the image's format or resolution */
static void test_the_fade_only_when_the_picture_changes(void)
{
    Fake f;
    const int keys[] = { RIGHT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    rig(&f, 320, ROW_CHOICE, 1);
    f.look = 1;
    run(&f, keys, 12);
    CHECK_EQ_INT(f.applies, 1);
    CHECK_EQ_INT(f.fade_outs, 1);
    CHECK_EQ_INT(f.fade_ins, 1);
    rig(&f, 320, ROW_CHOICE, 1);
    run(&f, keys, 12);
    CHECK_EQ_INT(f.applies, 1);
    CHECK_EQ_INT(f.fade_outs, 0);
    CHECK_EQ_INT(f.fade_ins, 0);
}

/* a click on the selector's left third shows the previous, on the right third the next; on the name it applies what is shown */
static void test_clicks_show_and_the_name_applies(void)
{
    Fake f;
    const int idle[] = { 0, 0, 0, 0 };
    rig(&f, 60, ROW_CHOICE, 1);
    f.clicks = 1 << 3;
    run(&f, idle, 4);
    CHECK_EQ_INT(f.last_peek, 1);
    CHECK_EQ_INT(f.applies, 0);   /* not yet: the arrows have not rested */
    rig(&f, 600, ROW_CHOICE, 1);
    f.clicks = 1 << 3;
    run(&f, idle, 4);
    CHECK_EQ_INT(f.last_peek, 3);
    const int right[] = { RIGHT, 0, 0, 0 };
    rig(&f, 320, ROW_CHOICE, 1);
    f.clicks = 1 << 5;   /* the click on the name: the choice shown applies without waiting */
    run(&f, right, 4);
    CHECK_EQ_INT(f.applies, 1);
    CHECK_EQ_INT(f.last_apply, 3);
}

/* owner 2026-11-25: Manage content is hidden in all three modes and an edition; Exit sits in its place, the third big row, and leaves; nothing opens the CONTENT screen */
static void test_manage_is_hidden(void)
{
    Fake f;
    const int enter[] = { ENTER };
    for (int in_use = 0; in_use <= 3; in_use++) {
        rig(&f, 320, ROW_EXIT, 1);
        f.manage_answer = 4;   /* a CONTENT screen exists in the rig */
        f.in_use = in_use;
        CHECK_EQ_INT(run(&f, enter, 1), 1);   /* Return on the third row is Exit */
        CHECK_EQ_INT(f.manages, 0);
        CHECK_EQ_INT(f.manage_banner, 0);
        CHECK_EQ_INT(f.manage.set, 0);
        CHECK_EQ_INT(f.exit.set, 1);
        CHECK_EQ_INT(f.names_asked, 0);
    }
}

/* dep59 step 4, T7: in an edition Multiplayer is dimmed; Return on it says why and opens nothing; in the standard edition it is as before */
static void test_multiplayer_in_an_edition(void)
{
    Fake f;
    const int enter[] = { ENTER, ESCAPE };
    rig(&f, 320, ROW_MULTI, 1);
    f.network_why = "Multiplayer needs the standard edition";
    run(&f, enter, 2);
    CHECK_EQ_INT(f.network_banner, 1);
    CHECK(f.timer_after_banner);   /* as Manage's: the deadline set again after the banner */
    CHECK_EQ_INT(f.result, GAME_MODE_RESULT_LEAVE);   /* Escape's, not the demo's time out */
    CHECK_EQ_INT(f.networks_asked + f.names_asked, 0);
    CHECK_EQ_INT(f.multi_style, STYLE_DIM);
    rig(&f, 320, ROW_MULTI, 1);
    run(&f, enter, 2);
    CHECK_EQ_INT(f.networks_asked, 1);
    CHECK(f.multi_style != STYLE_DIM);
}

/* the rows' rects: Return on each does its row's work; a click outside every rect (the version, the empty left of the bottom) does nothing */
static void test_the_pointer_rects(void)
{
    Fake f;
    const int enter[] = { ENTER };
    rig(&f, EXIT_X, ROW_EXIT, 1);
    CHECK_EQ_INT(run(&f, enter, 1), 1);   /* Exit leaves */
    CHECK_EQ_INT(f.key_calls, 2);
    rig(&f, 320, ROW_SINGLE, 1);
    CHECK_EQ_INT(run(&f, enter, 1), 0);   /* Single Player: the name box, then the race */
    CHECK_EQ_INT(f.names_asked, 1);
    rig(&f, 320, ROW_MULTI, 1);
    run(&f, enter, 1);
    CHECK_EQ_INT(f.networks_asked, 1);
    const int idle[] = { 0, 0, 0, 0 };
    rig(&f, 30, 455, 1);   /* on the version */
    f.clicks = 1 << 3;
    run(&f, idle, 4);
    CHECK_EQ_INT(f.names_asked + f.networks_asked + f.opens + f.banners, 0);
    CHECK_EQ_INT(f.exits_asked, 1);   /* only the script's Escape at its end */
    rig(&f, 320, 470, 1);   /* the bottom's middle: nothing */
    f.clicks = 1 << 3;
    run(&f, idle, 4);
    CHECK_EQ_INT(f.names_asked + f.networks_asked + f.opens + f.banners, 0);
    rig(&f, 320, GAP, 1);   /* the gap under Exit: nothing */
    f.clicks = 1 << 3;
    run(&f, idle, 4);
    CHECK_EQ_INT(f.names_asked + f.networks_asked + f.opens + f.banners, 0);
}

/* the version and the update notice: one foot line in the small size, the version first and the notice after two spaces' width */
static void test_the_foot_line(void)
{
    Fake f;
    const int none[] = { ESCAPE };
    rig(&f, 320, ROW_SINGLE, 0);
    run(&f, none, 1);
    CHECK(f.version.y + 20 <= 480 - 10);
    CHECK_EQ_INT(f.notice.y, f.version.y);
    CHECK_EQ_INT(f.notice.x, f.version.x + f.version.w + 2 * 16 * 3 / 5);
}

/* a click opens the releases page only on the notice's own text */
static int opens_at(int x, int y)
{
    Fake f;
    const int idle[] = { 0, 0, 0, 0 };
    rig(&f, x, y, 1);
    f.clicks = 1 << 3;
    run(&f, idle, 4);
    return f.opens;
}

static void test_the_notice_is_its_own_text(void)
{
    Fake f;
    const int none[] = { ESCAPE };
    rig(&f, 320, ROW_SINGLE, 0);
    run(&f, none, 1);
    const int x = f.notice.x, w = f.notice.w, y = f.notice.y;
    CHECK(w > 0);
    CHECK_EQ_INT(opens_at(x, y + 5), 1);
    CHECK_EQ_INT(opens_at(x + w - 1, y + 5), 1);
    CHECK_EQ_INT(opens_at(x + w, y + 5), 0);
    CHECK_EQ_INT(opens_at(x - 1, y + 5), 0);
    CHECK_EQ_INT(opens_at(f.version.x + 2, y + 5), 0);
    CHECK_EQ_INT(opens_at(x + 4, y - 1), 0);
    CHECK_EQ_INT(opens_at(x + 4, y + 21), 0);
}

/* the notice lights under the pointer and goes plain off it; the version never lights */
static void test_the_notice_lights_under_the_pointer(void)
{
    Fake f;
    const int none[] = { ESCAPE };
    rig(&f, 320, ROW_SINGLE, 0);
    run(&f, none, 1);
    const int x = f.notice.x, y = f.notice.y;
    CHECK(f.notice_plain > 0);
    CHECK_EQ_INT(f.notice_hot, 0);
    const int idle[] = { 0, 0, 0, 0, 0, 0 };
    rig(&f, 320, ROW_SINGLE, 1);
    f.move_at = 6, f.move_x = x + 4, f.move_y = y + 5;
    run(&f, idle, 6);
    CHECK(f.notice_hot > 0 && f.notice_last != 0);
    rig(&f, x + 4, y + 5, 1);
    f.move_at = 6, f.move_x = 320, f.move_y = ROW_SINGLE;
    run(&f, idle, 6);
    CHECK(f.notice_hot > 0);
    CHECK_EQ_INT(f.notice_last, 0);
    CHECK_EQ_INT(f.version_hot, 0);
}

int main(void)
{
    test_the_order();
    test_the_layout();
    test_the_arrows_apply_once_they_rest();
    test_it_waits_for_the_arrows_to_rest();
    test_the_selector_goes_round();
    test_a_choice_that_cannot_be_used();
    test_leaving_the_row_applies();
    test_the_fade_only_when_the_picture_changes();
    test_clicks_show_and_the_name_applies();
    test_manage_is_hidden();
    test_multiplayer_in_an_edition();
    test_the_pointer_rects();
    test_the_foot_line();
    test_the_notice_is_its_own_text();
    test_the_notice_lights_under_the_pointer();
    return TEST_REPORT();
}
