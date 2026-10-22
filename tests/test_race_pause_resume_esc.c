/* The race's menu/pause keys (race_schedule.c): after a pause-return, the next ESC must schedule the in-race
 * menu without waiting for the menu-gap window. The pause path (race_pause) clears `pending` and resets
 * `last` the same way the start-of-race reset does; this test pins both branches. */
#include "game/race/race_schedule.h"
#include "test.h"

#include <stddef.h>
#include <string.h>

enum { PHASE_RACING = 1 };

/* ----- ops capture ----- */
typedef struct {
    int set_calls;
    int16_t set_frame;
    int32_t set_which;
    int detail_calls;
    int32_t detail_segments;
} MenuTrack;

typedef struct {
    int sound_pause_calls;
    int voices_off_calls;
    int end_43cbcc_calls;
    int hud_clear_calls;
    int take_calls;
    int32_t take_kind;
    int music_on_calls;
    int setup_43cbe0_calls;
    int net_wait_calls;
    int32_t net_wait_kind;

    /* mutable pause-loop state: race_pause re-reads `in->pause` between service calls. The fake flips it
     * 0 → 1 on the second call so the three loops close. */
    RacePauseInputs *pinned_in;
    int service_calls;
    uint8_t pause_sequence[8];
    int pause_len;
} PauseTrack;

/* ----- RaceMenuKeysOps ----- */
static void tk_menu_set(void *ctx, int16_t frame, int32_t which, int16_t arg)
{
    MenuTrack *t = (MenuTrack *)ctx;
    t->set_calls++;
    t->set_frame = frame;
    t->set_which = which;
    (void)arg;
}
static void tk_menu_detail(void *ctx, int32_t segments)
{
    MenuTrack *t = (MenuTrack *)ctx;
    t->detail_calls++;
    t->detail_segments = segments;
}
static int32_t tk_no_notice(void *ctx) { (void)ctx; return 0; }

/* ----- RacePauseOps ----- */
static void tk_sound_pause(void *ctx) { ((PauseTrack *)ctx)->sound_pause_calls++; }
static void tk_voices_off(void *ctx) { ((PauseTrack *)ctx)->voices_off_calls++; }
static void tk_end_43cbcc(void *ctx) { ((PauseTrack *)ctx)->end_43cbcc_calls++; }
static void tk_movie(void *ctx) { (void)ctx; }
static void tk_hud_clear(void *ctx) { ((PauseTrack *)ctx)->hud_clear_calls++; }
static void tk_take(void *ctx, int32_t kind)
{
    PauseTrack *t = (PauseTrack *)ctx;
    t->take_calls++;
    t->take_kind = kind;
}
static void tk_wait_screen(void *ctx) { (void)ctx; }
static void tk_net_send(void *ctx, int32_t to) { (void)ctx; (void)to; }
static int32_t tk_net_wait(void *ctx, int32_t kind)
{
    PauseTrack *t = (PauseTrack *)ctx;
    t->net_wait_calls++;
    t->net_wait_kind = kind;
    return 1;
}
static void tk_music_on(void *ctx) { ((PauseTrack *)ctx)->music_on_calls++; }
static void tk_setup_43cbe0(void *ctx) { ((PauseTrack *)ctx)->setup_43cbe0_calls++; }

static void tk_service_pause_walk(void *ctx)
{
    PauseTrack *t = (PauseTrack *)ctx;
    t->service_calls++;
    if (t->service_calls - 1 < t->pause_len) t->pinned_in->pause = t->pause_sequence[t->service_calls - 1];
}

/* Start-of-race reset is the regression baseline: tick in the KEYS_RESET_BEFORE window clears pending + last. */
static void start_of_race_resets_last(void)
{
    RaceMenuKeysState st = { .last = 12345, .pending = 1 };
    MenuTrack mt = { 0 };
    RaceMenuKeysOps mops = { .ctx = &mt, .notice_up = tk_no_notice, .set = tk_menu_set, .detail = tk_menu_detail };
    RaceMenuKeysInputs in = { .tick = 0x10, .phase = PHASE_RACING, .esc = 0, .pause = 0, .lshift = 0, .rshift = 0,
                              .playback = 0, .replay = 0, .detail = 0 };
    race_menu_keys(&mops, &in, &st, (int16_t)in.tick);
    CHECK_EQ_INT(st.last, 0);
    CHECK_EQ_INT(st.pending, 0);
}

/* Sanity: without a pause, the menu-gap window still blocks a re-open. */
static void menu_gap_still_blocks_without_pause(void)
{
    RaceMenuKeysState st = { .last = 0, .pending = 0 };
    MenuTrack mt = { 0 };
    RaceMenuKeysOps mops = { .ctx = &mt, .notice_up = tk_no_notice, .set = tk_menu_set, .detail = tk_menu_detail };
    RaceMenuKeysInputs in = { .tick = 5000, .phase = PHASE_RACING, .esc = 1, .pause = 0, .lshift = 0, .rshift = 0,
                              .playback = 0, .replay = 0, .detail = 0 };
    race_menu_keys(&mops, &in, &st, (int16_t)in.tick);
    CHECK_EQ_INT(mt.set_calls, 1);
    CHECK_EQ_INT(st.pending, 1);
    CHECK_EQ_INT(st.last, 5000);

    /* Second ESC 50 ticks later: blocked. */
    RaceMenuKeysInputs in2 = { .tick = 5050, .phase = PHASE_RACING, .esc = 1, .pause = 0, .lshift = 0, .rshift = 0,
                               .playback = 0, .replay = 0, .detail = 0 };
    race_menu_keys(&mops, &in2, &st, (int16_t)in2.tick);
    CHECK_EQ_INT(mt.set_calls, 1);
    CHECK_EQ_INT(st.pending, 1);
}

/* The core regression. Going into the pause: last carries a previous menu-schedule tick and pending is 0.
 * After race_pause returns: pending stays cleared, and last must be 0 (the bug was `st->last = in->tick`).
 *
 * To walk race_pause's three pause-loops we drive `in->pause` through a deterministic sequence: each call
 * to the service callback flips pause according to the script. The mirror is non-const because race_pause
 * re-reads `in->pause` between service calls. */
static void pause_return(int port, RaceMenuKeysState *out)
{
    const int32_t earlier_last = 5000;
    RaceMenuKeysState st = { .last = earlier_last, .pending = 0 };

    RacePauseInputs in_storage = { .pause = 1, .movie = 0, .follow_kind = 0x1234, .w4fb7d4 = 0,
                                   .w524e9f = 0, .tick = 5050, .port_resume_keys = port };
    PauseTrack pt;
    memset(&pt, 0, sizeof pt);
    pt.pinned_in = &in_storage;
    pt.pause_sequence[0] = 0;     /* service 1: exit the first pause_while_down loop; into the second. */
    pt.pause_sequence[1] = 0;     /* service 2: `while(pause==0) pause_serve` already exited? No — it stays */
                                  /* in pause_serve until pause goes 1. Sequence: 1, then 0 makes both */
                                  /* transition. Use [1, 0, 1, 0]. */
    pt.pause_sequence[0] = 0;     /* service 1 (in loop 1, while pause != 0): release P → exit loop 1 */
    pt.pause_sequence[1] = 1;     /* service 2 (in loop 2, while pause == 0): press P → exit loop 2 */
    pt.pause_sequence[2] = 0;     /* service 3 (in loop 3, while pause != 0): release P → exit loop 3 */
    pt.pause_len = 3;

    RacePauseOps pops = { .ctx = &pt, .sound_pause = tk_sound_pause, .voices_off = tk_voices_off,
                          .end_43cbcc = tk_end_43cbcc, .movie = tk_movie, .service = tk_service_pause_walk,
                          .hud_clear = tk_hud_clear, .take = tk_take, .wait_screen = tk_wait_screen,
                          .net_send = tk_net_send, .net_wait = tk_net_wait, .music_on = tk_music_on,
                          .setup_43cbe0 = tk_setup_43cbe0 };

    int32_t loading = 0;
    race_pause(&pops, &in_storage, &st, &loading);
    /* Confirm the pause plumbing ran the expected pieces. */
    CHECK_EQ_INT(pt.sound_pause_calls, 1);
    CHECK_EQ_INT(pt.voices_off_calls, 1);
    CHECK_EQ_INT(pt.end_43cbcc_calls, 1);
    CHECK_EQ_INT(pt.hud_clear_calls, 1);
    CHECK_EQ_INT(pt.take_calls, 1);
    CHECK_EQ_INT(pt.take_kind, 0x1234);
    CHECK_EQ_INT(pt.music_on_calls, 1);
    CHECK_EQ_INT(pt.setup_43cbe0_calls, 1);
    CHECK_EQ_INT(st.pending, 0);
    *out = st;
}

/* the exe writes last = the tick on the way out (ESC dead for MENU_GAP after); the port's flag clears it */
static void pause_return_allows_immediate_esc(void)
{
    RaceMenuKeysState st;
    pause_return(0, &st);
    CHECK_EQ_INT(st.last, 5050);
    pause_return(1, &st);
    CHECK_EQ_INT(st.last, 0);

    /* Follow-up: ESC at tick 5100 (inside MENU_GAP=180 of 5050). With the fix, last=0 lets it through. */
    MenuTrack mt = { 0 };
    RaceMenuKeysOps mops = { .ctx = &mt, .notice_up = tk_no_notice, .set = tk_menu_set, .detail = tk_menu_detail };
    RaceMenuKeysInputs kin = { .tick = 5100, .phase = PHASE_RACING, .esc = 1, .pause = 0, .lshift = 0, .rshift = 0,
                               .playback = 0, .replay = 0, .detail = 0 };
    race_menu_keys(&mops, &kin, &st, (int16_t)kin.tick);
    CHECK_EQ_INT(mt.set_calls, 1);
    CHECK_EQ_INT(mt.set_which, RACE_SCHEDULE_MENU);
    CHECK_EQ_INT((int)mt.set_frame, kin.tick + 1);
    CHECK_EQ_INT(st.pending, 1);
    CHECK_EQ_INT(st.last, 5100);
}

/* The ESC menu's way out (004557e8 after race_menu): the exe stores the tick, so ESC is dead for MENU_GAP ticks;
 * the port clears it and the next ESC opens the menu again at once. */
static void menu_return(int port)
{
    RaceMenuKeysState st = { .last = 5000, .pending = 0 };
    race_menu_keys_resumed(&st, 5000, port);
    MenuTrack mt = { 0 };
    RaceMenuKeysOps mops = { .ctx = &mt, .notice_up = tk_no_notice, .set = tk_menu_set, .detail = tk_menu_detail };
    RaceMenuKeysInputs in = { .tick = 5010, .phase = PHASE_RACING, .esc = 1, .pause = 0, .lshift = 0, .rshift = 0,
                              .playback = 0, .replay = 0, .detail = 0 };
    race_menu_keys(&mops, &in, &st, (int16_t)in.tick);
    CHECK_EQ_INT(mt.set_calls, port ? 1 : 0);
}

static void menu_return_allows_immediate_esc(void)
{
    menu_return(1);
    menu_return(0);
}

int main(void)
{
    start_of_race_resets_last();
    menu_gap_still_blocks_without_pause();
    pause_return_allows_immediate_esc();
    menu_return_allows_immediate_esc();
    return TEST_REPORT();
}
