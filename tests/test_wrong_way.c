/* The wrong-way warning's judgement (wrong_way.h): on after 1.5 s driving forwards with the nose more than a quarter turn from the road,
 * at speed and not spinning; reversing never, whichever way the nose points; off after 0.5 s of anything else; never in a countdown, after
 * the finish or in a replay; blinking at 2 Hz. */
#include "game/race/wrong_way.h"

#include "test.h"

enum { TURN = 0x1000000, DEG = TURN / 360, FAST = 20 << 16, STEP = 33 };

static WrongWayInput drive(int32_t road, int32_t nose, int32_t forward)
{
    return (WrongWayInput){ .running = 1, .road_yaw = road, .nose_yaw = nose, .forward_speed = forward };
}

static WrongWay run(WrongWay w, WrongWayInput in, int ms)
{
    for (int t = 0; t < ms; t += STEP) wrong_way_step(&w, &in, STEP);
    return w;
}

static void test_forwards_against_the_road_turns_it_on_after_a_while(void)
{
    const WrongWay w0 = { 0 };
    CHECK(!run(w0, drive(0, 180 * DEG, FAST), 1400).on);   /* not yet */
    CHECK(run(w0, drive(0, 180 * DEG, FAST), 1600).on);
    CHECK(run(w0, drive(0, 100 * DEG, FAST), 1600).on);   /* past a quarter turn */
    CHECK(run(w0, drive(0, (360 - 100) * DEG, FAST), 1600).on);   /* the other side */
}

static void test_ahead_slow_or_spinning_never(void)
{
    const WrongWay w0 = { 0 };
    CHECK(!run(w0, drive(0, 80 * DEG, FAST), 5000).on);
    CHECK(!run(w0, drive(0, 0, FAST), 5000).on);
    CHECK(!run(w0, drive(0, 180 * DEG, 0), 5000).on);   /* stopped */
    CHECK(!run(w0, drive(0, 180 * DEG, 5 << 16), 5000).on);   /* 5 m/s */
    WrongWayInput spin = drive(0, 180 * DEG, FAST);
    spin.yaw_rate = 0x600000;
    CHECK(!run(w0, spin, 5000).on);
    spin.yaw_rate = -0x600000;
    CHECK(!run(w0, spin, 5000).on);
    WrongWayInput held = drive(0, 180 * DEG, FAST);
    held.running = 0;   /* the countdown, the finish, a replay */
    CHECK(!run(w0, held, 5000).on);
}

/* reversing is manoeuvring: never, whichever way the nose points */
static void test_reversing_never(void)
{
    const WrongWay w0 = { 0 };
    WrongWayInput back = drive(0, 0, -FAST);   /* nose along the road, going backwards (against it) */
    back.reverse_gear = 1;
    CHECK(!run(w0, back, 5000).on);
    back.nose_yaw = 180 * DEG;   /* nose against the road, backing along it */
    CHECK(!run(w0, back, 5000).on);
    back.reverse_gear = 0;   /* rolling backwards in a forward gear: still not forwards */
    CHECK(!run(w0, back, 5000).on);
    WrongWayInput gear_only = drive(0, 180 * DEG, FAST);   /* reverse engaged while still rolling forwards: not driving forwards */
    gear_only.reverse_gear = 1;
    CHECK(!run(w0, gear_only, 5000).on);
}

static void test_a_short_moment_wrong_is_forgotten(void)
{
    WrongWay w = run((WrongWay){ 0 }, drive(0, 180 * DEG, FAST), 1000);
    w = run(w, drive(0, 0, FAST), 100);   /* right again: the count starts over */
    CHECK(!run(w, drive(0, 180 * DEG, FAST), 1000).on);
}

static void test_it_goes_after_half_a_second_right(void)
{
    WrongWay w = run((WrongWay){ 0 }, drive(0, 180 * DEG, FAST), 1600);
    CHECK(w.on);
    CHECK(run(w, drive(0, 0, FAST), 400).on);   /* not yet */
    CHECK(!run(w, drive(0, 0, FAST), 600).on);
    WrongWayInput held = drive(0, 180 * DEG, FAST);
    held.running = 0;
    CHECK(!run(w, held, STEP).on);   /* the finish, a replay: at once */
    w = run(w, drive(0, 0, FAST), 300);   /* a glimpse right, then wrong again: kept */
    w = run(w, drive(0, 180 * DEG, FAST), STEP);
    CHECK(run(w, drive(0, 0, FAST), 400).on);
}

/* the road's heading near the circuit's start (node 0 after the last) is an angle like any: 350 against 10 is 20 apart */
static void test_angles_wrap(void)
{
    const WrongWay w0 = { 0 };
    CHECK(!run(w0, drive(350 * DEG, 10 * DEG, FAST), 3000).on);
    CHECK(!run(w0, drive(10 * DEG, 350 * DEG, FAST), 3000).on);
    CHECK(run(w0, drive(350 * DEG, 170 * DEG, FAST), 3000).on);
    CHECK(!run(w0, drive((int32_t)(TURN + 350 * DEG), 10 * DEG, FAST), 3000).on);   /* bits over the 24 ignored */
}

static void test_blink_and_words(void)
{
    WrongWay w = run((WrongWay){ 0 }, drive(0, 180 * DEG, FAST), 1500 + STEP);
    CHECK(w.on);
    CHECK(wrong_way_visible(&w));   /* lit as it comes up */
    int lit = 0, dark = 0, changes = 0, was = 1;
    const WrongWayInput in = drive(0, 180 * DEG, FAST);
    for (int t = 0; t < 2000; t += STEP) {
        wrong_way_step(&w, &in, STEP);
        const int v = wrong_way_visible(&w);
        if (v) lit++; else dark++;
        changes += v != was, was = v;
    }
    CHECK(lit > 20 && dark > 20);
    CHECK(changes >= 7 && changes <= 9);   /* 2 s at 2 Hz: 8 changes */
    CHECK(!wrong_way_visible(&(WrongWay){ 0 }));
    CHECK(strcmp(wrong_way_text(0), "WRONG WAY") == 0);
    CHECK(strcmp(wrong_way_text(1), "FALSCHE RICHTUNG") == 0);
}

int main(void)
{
    test_forwards_against_the_road_turns_it_on_after_a_while();
    test_ahead_slow_or_spinning_never();
    test_reversing_never();
    test_a_short_moment_wrong_is_forgotten();
    test_it_goes_after_half_a_second_right();
    test_angles_wrap();
    test_blink_and_words();
    return TEST_REPORT();
}
