/* the weather's hush (hush.h): the pause and the in-race menu silence the thunder, the rain and the roll on snow within 30 ms, with no step; awake when the race's clock moves */
#include "tests/test.h"

#include "game/audio/hush.h"

enum { RATE = 0x5622 };   /* the mixer's (drive_audio.c) */

/* `frames` frames' gains, in blocks as the device's thread mixes; the last one */
static int32_t run(Hush *h, int frames)
{
    int32_t gain[64];
    int32_t last = -1;
    for (int done = 0; done < frames;) {
        const int n = frames - done < 64 ? frames - done : 64;
        hush_gains(h, gain, (uint32_t)n, RATE);
        last = gain[n - 1];
        done += n;
    }
    return last;
}

/* not hushed: whole, every frame (the race's sound as it was) */
static void test_whole_unless_hushed(void)
{
    Hush h = { 0 };
    int32_t gain[300];
    hush_gains(&h, gain, 300, RATE);
    for (int i = 0; i < 300; i++) CHECK_EQ_INT(gain[i], HUSH_UNITY);
    CHECK(!hush_silent(&h));
}

/* hushed: down a little each frame, silent after more than 10 ms and within 30, and held there */
static void test_quick_and_smooth(void)
{
    Hush h = { 0 };
    hush_at(&h, 77);
    int32_t gain[2048];
    hush_gains(&h, gain, 2048, RATE);
    int silent_at = -1;
    for (int i = 0; i < 2048; i++) {
        if (i > 0) CHECK(gain[i] <= gain[i - 1] && gain[i - 1] - gain[i] <= HUSH_UNITY / 300);   /* down, no click */
        if (silent_at < 0 && gain[i] == 0) silent_at = i;
    }
    CHECK(silent_at > RATE * 10 / 1000 && silent_at <= RATE * 30 / 1000);
    CHECK(hush_silent(&h));
    CHECK_EQ_INT(run(&h, RATE), 0);   /* a second later still nothing */
}

/* the race moving again wakes it (not a frame drawn while the clock stands still: the menu's own); back as quickly and as smoothly */
static void test_wakes_with_the_race(void)
{
    Hush h = { 0 };
    CHECK(!hush_wake(&h, 5));   /* never hushed: nothing to wake */
    hush_at(&h, 500);
    run(&h, RATE / 10);
    CHECK(!hush_wake(&h, 500) && hush_silent(&h));   /* the same tick: the pause, the menu, a pause then the menu */
    hush_at(&h, 500);
    CHECK(!hush_wake(&h, 500));
    CHECK(hush_wake(&h, 501) && !hush_silent(&h));
    CHECK(!hush_wake(&h, 502));   /* once */
    int32_t gain[700];
    hush_gains(&h, gain, 700, RATE);
    for (int i = 1; i < 700; i++) CHECK(gain[i] >= gain[i - 1] && gain[i] - gain[i - 1] <= HUSH_UNITY / 300);
    CHECK_EQ_INT(gain[RATE * 30 / 1000], HUSH_UNITY);   /* whole again within 30 ms */
    hush_at(&h, 900);
    CHECK(hush_wake(&h, 10));   /* a clock gone back (a replay, a new race) moves too */
}

int main(void)
{
    test_whole_unless_hushed();
    test_quick_and_smooth();
    test_wakes_with_the_race();
    return TEST_REPORT();
}
