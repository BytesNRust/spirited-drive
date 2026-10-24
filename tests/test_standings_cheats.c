/* The standings screens after a race (00461540) with fake ops: the "(CHEATS)" banner is in the header band, right of the
 * Road & Track logo, on the segment's ranking and on the totals' standings of a race with cheats, and on neither of a
 * normal race. */
#include "game/frontend/standings.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

enum { DRAWN_MAX = 256, TEXT_MAX = 40, BANNER_X = 0x122, BANNER_Y = 0x14 };

typedef struct Drawn {
    char text[DRAWN_MAX][TEXT_MAX];
    int32_t x[DRAWN_MAX], y[DRAWN_MAX];
    int count;
} Drawn;

static Drawn drawn;
static uint8_t palette[0x300];

static void record(const char *text, int32_t x, int32_t y)
{
    if (drawn.count >= DRAWN_MAX) return;
    snprintf(drawn.text[drawn.count], TEXT_MAX, "%s", text);
    drawn.x[drawn.count] = x, drawn.y[drawn.count] = y;
    drawn.count++;
}

static void f_text_boxed(void *ctx, const char *text, int32_t x, int32_t y) { (void)ctx; record(text, x, y); }
static void f_time_text(void *ctx, char *out, int32_t time) { (void)ctx, (void)time; snprintf(out, 0x5c, "t"); }
static void f_nothing(void *ctx) { (void)ctx; }
static int32_t f_zero(void *ctx) { (void)ctx; return 0; }
static int32_t f_load(void *ctx, const char *name) { (void)ctx, (void)name; return 0; }
static const uint8_t *f_shape(void *ctx, int32_t bank, const char *tag)
{
    static const uint8_t blank[0x400] = { 0 };
    (void)ctx, (void)bank, (void)tag;
    return blank;
}
static void f_palette(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)ctx, (void)first, (void)count, (void)rgb;
}
static void f_blit(void *ctx, const uint8_t *shape) { (void)ctx, (void)shape; }
static void f_blit_word(void *ctx, int32_t shape) { (void)ctx, (void)shape; }
static void f_select(void *ctx, int32_t picture) { (void)ctx, (void)picture; }
static void f_text_select(void *ctx, int32_t style) { (void)ctx, (void)style; }
static void f_move_to(void *ctx, int32_t x, int32_t y) { (void)ctx, (void)x, (void)y; }
static int32_t f_in_rect(void *ctx, int32_t x0, int32_t x1, int32_t y0, int32_t y1)
{
    (void)ctx, (void)x0, (void)x1, (void)y0, (void)y1;
    return 0;
}
static void f_results_order(void *ctx) { (void)ctx; }
static void f_pointer_mark(void *ctx, DialogPointer *p) { (void)ctx, (void)p; }
static void f_fade_in(void *ctx, int32_t first, int32_t count, const uint8_t *rgb)
{
    (void)ctx, (void)first, (void)count, (void)rgb;
}
static void f_fade_out(void *ctx, int32_t first, int32_t count, int32_t bank)
{
    (void)ctx, (void)first, (void)count, (void)bank;
}
static void f_delay(void *ctx, int32_t time) { (void)ctx, (void)time; }
static void f_sound(void *ctx, int32_t sound) { (void)ctx, (void)sound; }
static int32_t f_key(void *ctx) { (void)ctx; return 0x0d; }   /* Enter: the screen ends after its first draw */
static void f_free(void *ctx, int32_t bank) { (void)ctx, (void)bank; }

static StandingsOps fake_ops(void)
{
    StandingsOps ops;
    memset(&ops, 0, sizeof ops);
    ops.flush = (void (*)(void *))f_nothing;
    ops.pointer_move_to = f_move_to;
    ops.load_art = f_load;
    ops.load_inverse = f_load;
    ops.music_feed = (void (*)(void *))f_nothing;
    ops.shape = f_shape;
    ops.palette_set = f_palette;
    ops.palette_write = f_palette;
    ops.inverse_build = (void (*)(void *))f_nothing;
    ops.text_styles = (void (*)(void *))f_nothing;
    ops.pointer_poll = (void (*)(void *))f_nothing;
    ops.pointer_hide = (void (*)(void *))f_nothing;
    ops.pointer_draw = (void (*)(void *))f_nothing;
    ops.pointer_in_rect = f_in_rect;
    ops.select = f_select;
    ops.select_default = (void (*)(void *))f_nothing;
    ops.blit = f_blit;
    ops.blit_masked = f_blit;
    ops.blit_word = f_blit_word;
    ops.text_select = f_text_select;
    ops.text_boxed = f_text_boxed;
    ops.time_text = f_time_text;
    ops.results_order = f_results_order;
    ops.lock = (void (*)(void *))f_nothing;
    ops.unlock = (void (*)(void *))f_nothing;
    ops.flip = (void (*)(void *))f_nothing;
    ops.pointer_show = (void (*)(void *))f_nothing;
    ops.pointer_mark = f_pointer_mark;
    ops.pointer_buttons = f_zero;
    ops.fade_in = f_fade_in;
    ops.fade_out = f_fade_out;
    ops.delay = f_delay;
    ops.sound = f_sound;
    ops.ticks = f_zero;
    ops.pointer_activate_if_moved = (void (*)(void *))f_nothing;
    ops.key = f_key;
    ops.pointer_click = (void (*)(void *))f_nothing;
    ops.pointer_off = (void (*)(void *))f_nothing;
    ops.music_service = (void (*)(void *))f_nothing;
    ops.deadline_passed = f_zero;
    ops.free = f_free;
    ops.free_inverse = (void (*)(void *))f_nothing;
    return ops;
}

static int found(const char *text, int32_t x, int32_t y)
{
    for (int i = 0; i < drawn.count; i++)
        if (strcmp(drawn.text[i], text) == 0 && drawn.x[i] == x && drawn.y[i] == y) return 1;
    return 0;
}

static int any_text(const char *text)
{
    for (int i = 0; i < drawn.count; i++)
        if (strcmp(drawn.text[i], text) == 0) return 1;
    return 0;
}

/* one screen of the standings: a segment's ranking (segment_mode) or the totals, of a race with cheats or without */
static void run(int segment_mode, int cheats)
{
    static uint8_t game[STANDINGS_GAME];
    static const int32_t finish[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    static const int16_t car_points[8] = { 0 };
    StandingsInputs in;
    StandingsState st = { 0, palette };
    DialogPointer p;
    StandingsOps ops = fake_ops();
    memset(game, 0, sizeof game);
    memset(&in, 0, sizeof in);
    memset(&p, 0, sizeof p);
    in.game = game, in.finish = finish, in.car_points = car_points, in.track = 0, in.segment = 1, in.laps = 1;
    in.german = 0, in.level = 0, in.unknown_50a5b6 = 0, in.screen = 0, in.screen_shape = 0;
    in.cheats = cheats;
    drawn.count = 0;
    standings_screen(segment_mode, &in, &st, &p, &ops);
}

/* the segment's ranking ("Rank Name Total Time Segment Time") shows the banner on a race with cheats */
static void test_segment_ranking_banner_on_cheats_race(void)
{
    run(1, 1);
    CHECK(found("(CHEATS)", BANNER_X, BANNER_Y));
}

static void test_segment_ranking_has_no_banner_on_normal_race(void)
{
    run(1, 0);
    CHECK(!any_text("(CHEATS)"));
}

/* the totals' standings (the tournament's, or a finished track's) read the same way */
static void test_totals_standings_banner_on_cheats_race(void)
{
    run(0, 1);
    CHECK(found("(CHEATS)", BANNER_X, BANNER_Y));
}

static void test_totals_standings_has_no_banner_on_normal_race(void)
{
    run(0, 0);
    CHECK(!any_text("(CHEATS)"));
}

int main(void)
{
    test_segment_ranking_banner_on_cheats_race();
    test_segment_ranking_has_no_banner_on_normal_race();
    test_totals_standings_banner_on_cheats_race();
    test_totals_standings_has_no_banner_on_normal_race();
    return TEST_REPORT();
}
