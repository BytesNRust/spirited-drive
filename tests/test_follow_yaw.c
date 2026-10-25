/* port: the follow camera's yaw spring (follow_yaw.h) */
#include "game/race/follow_yaw.h"
#include "test.h"

#include <stdlib.h>

enum { QUARTER = 0x400000, HALF = 0x800000, FULL = 0x1000000 };

static int32_t wrapped(int32_t a)
{
    a &= FULL - 1;
    return a > HALF ? a - FULL : a;
}

/* a step to 90 degrees settles without overshooting by more than a hair */
static void test_step(void)
{
    FollowYaw s = { 0, 0 };
    int32_t peak = 0;
    for (int i = 0; i < 90; i++) {
        follow_yaw_step(&s, QUARTER, 0);
        if (s.yaw > peak) peak = s.yaw;
    }
    CHECK(abs(wrapped(QUARTER - s.yaw)) < QUARTER / 100);   /* 3 s in (90 ticks): within 1 % */
    CHECK(peak < QUARTER + QUARTER / 50);                   /* overshoot under 2 % */
    FollowYaw early = { 0, 0 };
    for (int i = 0; i < 10; i++) follow_yaw_step(&early, QUARTER, 0);
    CHECK(early.yaw > QUARTER / 50 && early.yaw < QUARTER / 2);   /* lags: a third of a second in, neither at once nor still */
}

/* a target 180 degrees off goes whichever way the target turns; at rest it is the positive way */
static void test_opposite(void)
{
    FollowYaw a = { 0, 0 }, b = { 0, 0 }, c = { 0, 0 };
    follow_yaw_step(&a, HALF, 100000);
    follow_yaw_step(&b, HALF, -100000);
    follow_yaw_step(&c, HALF, 0);
    CHECK(wrapped(a.yaw) > 0 && wrapped(b.yaw) < 0 && wrapped(c.yaw) > 0);
}

/* the shortest way across the wrap: from just below a full turn to just above zero goes up through it */
static void test_wrap(void)
{
    FollowYaw s = { FULL - 1000, 0 };
    for (int i = 0; i < 5; i++) follow_yaw_step(&s, 20000, 0);
    CHECK(wrapped(s.yaw) > -1000 + 1 || s.yaw < 20000);   /* moved up, not the long way round */
    CHECK(s.velocity > 0);
    FollowYaw t = { 1000, 0 };
    for (int i = 0; i < 5; i++) follow_yaw_step(&t, FULL - 20000, 0);
    CHECK(t.velocity < 0);
}

/* a target turning at a steady rate: the camera ends up moving with it at the same rate, a tick's turn ahead (it moves
 * after it has seen the target's new place), and does not lag behind as a spring toward a still target would */
static void test_steady_turn(void)
{
    FollowYaw s = { 0, 0 };
    const int32_t rate = 150000;   /* about 13 degrees a tick: a fast spin */
    int32_t target = 0;
    for (int i = 0; i < 200; i++) {
        target = (target + rate) & (FULL - 1);
        follow_yaw_step(&s, target, rate);
    }
    CHECK(abs(wrapped(target - s.yaw) + rate) < 2000);
    CHECK(abs(s.velocity - rate) < 2000);
}

/* the velocity is held to what a spin at the game's top turn rate needs, and the yaw stays a 24-bit angle */
static void test_limits(void)
{
    FollowYaw s = { 0, 0 };
    int32_t fastest = 0;
    for (int i = 0; i < 100; i++) {
        follow_yaw_step(&s, HALF, 5000000);
        CHECK(abs(s.velocity) <= FOLLOW_YAW_MAX_VELOCITY);
        CHECK(s.yaw >= 0 && s.yaw < FULL);
        if (s.velocity > fastest) fastest = s.velocity;
    }
    CHECK_EQ_INT(fastest, FOLLOW_YAW_MAX_VELOCITY);   /* held at the limit, not dropped */
    FollowYaw t = { 0, 0 };
    for (int i = 0; i < 100; i++) follow_yaw_step(&t, HALF, -5000000);
    CHECK(t.velocity >= -FOLLOW_YAW_MAX_VELOCITY && t.velocity < -FOLLOW_YAW_MAX_VELOCITY / 2);
}

int main(void)
{
    test_step();
    test_opposite();
    test_wrap();
    test_steady_turn();
    test_limits();
    return TEST_REPORT();
}
