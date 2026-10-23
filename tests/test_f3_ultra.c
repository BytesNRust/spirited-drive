/* The port's Ultra in F3's rotation (race_keys.h port_f3_ultra): with it 1, F3 goes Far (28) -> Medium (14) -> Close (9)
 * -> Ultra (56) -> Far, and the exe's 0x1c/0xe/9 every 3 presses with 0. Also the track's reach for the segments asked
 * (track_render_set_reach) and the race's View row in the pause menu (graphics_own_view_live). */
#include "game/frontend/graphics_own.h"
#include "game/race/race_detail.h"
#include "game/race/race_keys.h"
#include "game/race/track_render.h"
#include "test.h"

#include <string.h>

enum { F3 = 0x3d, SEGMENTS_FAR = 0x1c, SEGMENTS_MEDIUM = 0xe, SEGMENTS_CLOSE = 9 };

typedef struct {
    int32_t seen[16];
    int n;
} Log;

static void op_follow(void *ctx, int32_t slot) { (void)ctx, (void)slot; }
static void op_nop(void *ctx) { (void)ctx; }
static void op_reset(void *ctx, int32_t player) { (void)ctx, (void)player; }
static void op_f1(void *ctx) { (void)ctx; }
static void op_screen(void *ctx, int32_t kind) { (void)ctx, (void)kind; }
static void op_detail(void *ctx, int32_t value)
{
    Log *l = ctx;
    l->seen[l->n++] = value;
}
static void op_f5_on(void *ctx) { (void)ctx; }

/* one F3 press with `f3` the port flag, then the key let go (race_keys reads a press on its first frame only) */
static void press_f3(RaceKeysState *st, Log *log, int f3)
{
    const RaceKeysOps ops = { log, op_follow, op_nop, op_nop, op_nop, op_reset, op_f1, op_screen, op_detail, op_f5_on,
                              op_nop };
    uint8_t keys[256];
    RaceKeysInputs in;
    memset(keys, 0, sizeof keys);
    memset(&in, 0, sizeof in);
    in.keys = keys, in.humans = 1, in.high = 1, in.port_f3_ultra = f3;
    keys[F3] = 1;
    race_keys(&ops, st, &in);
    keys[F3] = 0;
    race_keys(&ops, st, &in);
}

static void test_exe_cycle_without_ultra(void)
{
    RaceKeysState st;
    Log log;
    memset(&log, 0, sizeof log);
    race_keys_start(&st, NULL);
    for (int i = 0; i < 6; i++) press_f3(&st, &log, 0);
    CHECK_EQ_INT(log.n, 6);
    const int32_t want[6] = { SEGMENTS_MEDIUM, SEGMENTS_CLOSE, SEGMENTS_FAR, SEGMENTS_MEDIUM, SEGMENTS_CLOSE, SEGMENTS_FAR };
    for (int i = 0; i < 6; i++) CHECK_EQ_INT(log.seen[i], want[i]);
    CHECK_EQ_INT(st.ultra, 0);
}

static void test_rotation_with_ultra(void)
{
    RaceKeysState st;
    Log log;
    memset(&log, 0, sizeof log);
    race_keys_start(&st, NULL);
    /* from the start (Far): Medium first, as the exe, then Close, Ultra, Far, ... */
    const int32_t want[8] = { SEGMENTS_MEDIUM, SEGMENTS_CLOSE, DRAW_SEGMENTS_ULTRA, SEGMENTS_FAR,
                              SEGMENTS_MEDIUM, SEGMENTS_CLOSE, DRAW_SEGMENTS_ULTRA, SEGMENTS_FAR };
    for (int i = 0; i < 8; i++) {
        press_f3(&st, &log, 1);
        CHECK_EQ_INT(log.seen[i], want[i]);
        CHECK_EQ_INT(st.ultra, want[i] == DRAW_SEGMENTS_ULTRA);
    }
    CHECK_EQ_INT(log.n, 8);
}

static void test_ultra_is_always_reachable(void)
{
    /* a race that starts on Ultra (the menu's) still goes Far after it, then Medium: F3 never loses Ultra */
    RaceKeysState st;
    Log log;
    memset(&log, 0, sizeof log);
    race_keys_start(&st, NULL);
    st.ultra = 1;
    st.detail = 0;
    press_f3(&st, &log, 1);
    CHECK_EQ_INT(log.seen[0], SEGMENTS_FAR);
    CHECK_EQ_INT(st.ultra, 0);
    press_f3(&st, &log, 1);
    CHECK_EQ_INT(log.seen[1], SEGMENTS_MEDIUM);
}

static void test_reach_follows_the_count(void)
{
    static TrackRender r;
    memset(&r, 0, sizeof r);
    track_render_set_reach(&r, SEGMENTS_FAR);  /* the exe's 28 is the default: no reach */
    CHECK_EQ_INT(r.reach, 0);
    CHECK_EQ_INT(r.draw_segments, 28);
    CHECK_EQ_INT(r.window.ahead, 30);
    track_render_set_reach(&r, SEGMENTS_CLOSE);
    CHECK_EQ_INT(r.draw_segments, 9);
    CHECK_EQ_INT(r.window.ahead, 11);
    track_render_set_reach(&r, SEGMENTS_MEDIUM);
    CHECK_EQ_INT(r.draw_segments, 14);
    track_render_set_reach(&r, DRAW_SEGMENTS_ULTRA);
    CHECK_EQ_INT(r.draw_segments, 56);
    CHECK_EQ_INT(r.window.ahead, 58);
    track_render_set_reach(&r, 99);  /* out of range: the exe's */
    CHECK_EQ_INT(r.draw_segments, 28);
    CHECK_EQ_INT(r.reach, 0);
}

static void test_reach_rebuilds_only_on_change(void)
{
    static TrackRender r;
    memset(&r, 0, sizeof r);
    track_render_set_reach(&r, SEGMENTS_CLOSE);
    r.build.first_node = 7;
    track_render_set_reach(&r, SEGMENTS_CLOSE);  /* the same count again: the track stays built */
    CHECK_EQ_INT(r.build.first_node, 7);
    track_render_set_reach(&r, SEGMENTS_MEDIUM);  /* a change: the next build starts again */
    CHECK_EQ_INT(r.build.first_node, -1);
}

static void test_view_row_live_in_race(void)
{
    /* with the port flag the View row is live in the pause menu, Ultra included */
    CHECK_EQ_INT(graphics_own_view_live(1, 0, 1), 1);
    /* without it the exe-era lock stays (Ultra shown "next race") */
    CHECK_EQ_INT(graphics_own_view_live(0, 0, 1), 0);
    CHECK_EQ_INT(graphics_own_view_live(0, 2, 0), 1);
}

static void test_view_step_in_race_reaches_ultra(void)
{
    int16_t view = 2;
    int ultra = 0;
    graphics_own_race_view_step_for(1, &view, &ultra);  /* Close -> Ultra */
    CHECK_EQ_INT(view, 0);
    CHECK_EQ_INT(ultra, 1);
    graphics_own_race_view_step_for(1, &view, &ultra);  /* Ultra -> Far */
    CHECK_EQ_INT(view, 0);
    CHECK_EQ_INT(ultra, 0);
    graphics_own_race_view_step_for(1, &view, &ultra);  /* Far -> Medium */
    CHECK_EQ_INT(view, 1);
    CHECK_EQ_INT(ultra, 0);
    view = 2;
    graphics_own_race_view_step_for(0, &view, &ultra);  /* the exe's race step: Close -> Far, no Ultra */
    CHECK_EQ_INT(view, 0);
    CHECK_EQ_INT(ultra, 0);
}

int main(void)
{
    test_exe_cycle_without_ultra();
    test_rotation_with_ultra();
    test_ultra_is_always_reachable();
    test_reach_follows_the_count();
    test_reach_rebuilds_only_on_change();
    test_view_row_live_in_race();
    test_view_step_in_race_reaches_ultra();
    return TEST_REPORT();
}
