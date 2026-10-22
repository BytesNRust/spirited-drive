/* The race's ESC after a replay (race_schedule.c): the ESC that closes the replay (ReplayInput.escape) is still down
 * when the race is back, and the exe opens the in-race menu with it at once; the port waits until it has been up. */
#include "game/race/race_schedule.h"
#include "test.h"

#include <stddef.h>

enum { PHASE_RACING = 1, PHASE_REPLAY = 3 };

static void set(void *ctx, int16_t frame, int32_t which, int16_t arg) { (void)frame, (void)which, (void)arg, ++*(int *)ctx; }
static void detail(void *ctx, int32_t segments) { (void)ctx, (void)segments; }
static int32_t no_notice(void *ctx) { return (void)ctx, 0; }

/* a race, F9's replay, and the race on again with ESC down: the menu opened by the way back, and by a later press */
static void replay_return(int port, int *at_once, int *later)
{
    RaceMenuKeysState st = { 0 };
    int opened = 0;
    const RaceMenuKeysOps ops = { &opened, no_notice, set, detail };
    RaceMenuKeysInputs in = { .tick = 5000, .phase = PHASE_RACING };
    race_menu_keys_phase(&st, in.phase == PHASE_RACING, in.tick, port);
    race_menu_keys(&ops, &in, &st, (int16_t)in.tick);
    in.phase = PHASE_REPLAY, in.tick = 1000;   /* the replay: the race's phase is not 1 */
    race_menu_keys_phase(&st, in.phase == PHASE_RACING, in.tick, port);
    in.phase = PHASE_RACING, in.tick = 5000, in.esc = 1;   /* Done by ESC: back with it down */
    race_menu_keys_phase(&st, in.phase == PHASE_RACING, in.tick, port);
    race_menu_keys(&ops, &in, &st, (int16_t)in.tick);
    *at_once = opened;
    st.pending = 0;
    in.esc = 0, in.tick = 5010;
    race_menu_keys_phase(&st, 1, in.tick, port);
    race_menu_keys(&ops, &in, &st, (int16_t)in.tick);
    in.esc = 1, in.tick = 5020;
    race_menu_keys(&ops, &in, &st, (int16_t)in.tick);
    *later = opened - *at_once;
}

static void esc_that_closed_the_replay(void)
{
    int at_once, later;
    replay_return(0, &at_once, &later);
    CHECK_EQ_INT(at_once, 1);   /* the exe: the same ESC opens the menu */
    replay_return(1, &at_once, &later);
    CHECK_EQ_INT(at_once, 0);
    CHECK_EQ_INT(later, 1);     /* a new press opens it at once */
}

/* the race starting (the countdown, then racing) is not a way back from a replay: nothing is armed */
static void race_start_arms_nothing(void)
{
    RaceMenuKeysState st = { 0 };
    race_menu_keys_phase(&st, 0, 0, 1);
    race_menu_keys_phase(&st, 1, 10, 1);
    CHECK_EQ_INT(st.port_esc_up_wait, 0);
}

int main(void)
{
    esc_that_closed_the_replay();
    race_start_arms_nothing();
    return TEST_REPORT();
}
