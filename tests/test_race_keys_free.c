/* The race's function keys with the port's EXTRAS keys (race_keys.h port_bumper_keys): F1 and F2 are the bumper
 * mirror's and radar's (drive_bumper) and F4 does nothing, so race_keys leaves all three alone; the pad's View button
 * (its virtual key RACE_KEYS_PAD_VIEW) still changes the view. 0 is the exe's: F1 the view, F2 the screen size, F4
 * the sky. F3, F5 and F6 are the exe's either way. */
#include "game/race/race_keys.h"
#include "test.h"

#include <string.h>

enum { F1 = 0x3b, F2 = 0x3c, F3 = 0x3d, F4 = 0x3e, F5 = 0x3f, F6 = 0x40 };

typedef struct {
    int f1, screen, detail, f5_on;
} Calls;

static void op_f1(void *ctx) { ((Calls *)ctx)->f1++; }
static void op_screen(void *ctx, int32_t kind) { (void)kind, ((Calls *)ctx)->screen++; }
static void op_detail(void *ctx, int32_t value) { (void)value, ((Calls *)ctx)->detail++; }
static void op_f5_on(void *ctx) { ((Calls *)ctx)->f5_on++; }
static void op_follow(void *ctx, int32_t slot) { (void)ctx, (void)slot; }
static void op_nop(void *ctx) { (void)ctx; }
static void op_reset(void *ctx, int32_t player) { (void)ctx, (void)player; }

/* one frame with `key` down (0 for none) on a fresh state, the port's keys `port` */
static RaceKeysState frame(int key, int port, Calls *calls)
{
    const RaceKeysOps ops = { calls, op_follow, op_nop, op_nop, op_nop, op_reset, op_f1, op_screen, op_detail, op_f5_on,
                              op_nop };
    uint8_t keys[256];
    memset(keys, 0, sizeof keys);
    if (key != 0) keys[key] = 1;
    RaceKeysInputs in;
    memset(&in, 0, sizeof in);
    in.keys = keys, in.humans = 1, in.high = 1, in.port_bumper_keys = port;
    RaceKeysState st;
    race_keys_start(&st, NULL);
    race_keys(&ops, &st, &in);
    return st;
}

static void test_exe_keys(void)
{
    Calls c;
    memset(&c, 0, sizeof c);
    frame(F1, 0, &c);
    CHECK_EQ_INT(c.f1, 1);
    frame(F2, 0, &c);
    CHECK_EQ_INT(c.screen, 1);
    CHECK_EQ_INT(frame(F4, 0, &c).w4fb786, 1);
    memset(&c, 0, sizeof c);
    frame(RACE_KEYS_PAD_VIEW, 0, &c);   /* the exe has no such key */
    CHECK_EQ_INT(c.f1, 0);
}

static void test_port_leaves_f1_f2_f4(void)
{
    Calls c;
    memset(&c, 0, sizeof c);
    RaceKeysState st = frame(F1, 1, &c);
    CHECK_EQ_INT(c.f1 + c.screen, 0);
    CHECK_EQ_INT(st.fkey_held, 0);
    st = frame(F2, 1, &c);
    CHECK_EQ_INT(c.f1 + c.screen, 0);
    CHECK_EQ_INT(st.screen_kind, 0);
    CHECK_EQ_INT(frame(F4, 1, &c).w4fb786, 0);
}

static void test_port_keeps_the_others(void)
{
    Calls c;
    memset(&c, 0, sizeof c);
    frame(F3, 1, &c);
    CHECK_EQ_INT(c.detail, 1);
    frame(F5, 1, &c);
    CHECK_EQ_INT(c.f5_on, 1);
    CHECK_EQ_INT(frame(F6, 1, &c).hidden, 1);
}

static void test_pad_view(void)
{
    Calls c;
    memset(&c, 0, sizeof c);
    frame(RACE_KEYS_PAD_VIEW, 1, &c);
    CHECK_EQ_INT(c.f1, 1);
}

int main(void)
{
    test_exe_keys();
    test_port_leaves_f1_f2_f4();
    test_port_keeps_the_others();
    test_pad_view();
    return TEST_REPORT();
}
