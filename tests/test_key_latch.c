/* The key latch (key_latch.h): a tap that came and went between two polls still reads down for two sim steps, then lets go. */
#include "game/sys/key_latch.h"
#include "test.h"

static void test_tap_lasts_two_steps(void)
{
    key_latch_clear();
    CHECK(!key_latch_down(80));
    key_latch_press(80);
    CHECK(key_latch_down(80));   /* the step that follows the pump */
    key_latch_step();
    CHECK(key_latch_down(80));   /* and the next: the exe polls every second step */
    key_latch_step();
    CHECK(!key_latch_down(80));  /* then the state alone decides */
}

static void test_keys_are_independent(void)
{
    key_latch_clear();
    key_latch_press(79);
    CHECK(key_latch_down(79));
    CHECK(!key_latch_down(80));
    key_latch_step();
    key_latch_press(80);   /* a later tap has its own two steps */
    key_latch_step();
    CHECK(!key_latch_down(79));
    CHECK(key_latch_down(80));
}

static void test_guards_and_clear(void)
{
    key_latch_press(-1);
    key_latch_press(KEY_LATCH_KEYS);
    CHECK(!key_latch_down(-1));
    CHECK(!key_latch_down(KEY_LATCH_KEYS));
    key_latch_press(5);
    key_latch_clear();
    CHECK(!key_latch_down(5));
}

/* a screen with no sim steps: the latch lets go after three pumps, not held for good */
static void test_pumps_expire_it(void)
{
    key_latch_clear();
    key_latch_press(80);
    key_latch_pump();
    key_latch_pump();
    CHECK(key_latch_down(80));
    key_latch_pump();
    CHECK(!key_latch_down(80));
}

int main(void)
{
    test_tap_lasts_two_steps();
    test_keys_are_independent();
    test_guards_and_clear();
    test_pumps_expire_it();
    return TEST_REPORT();
}
