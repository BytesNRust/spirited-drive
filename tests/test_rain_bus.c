/* where the rain's bus meets the race's mix (rain_bus.h): the plain sum below the knee, rounded softly above it, never past full scale, and only where the rain sounds; the moving
 * rain at half; hushed by the pause and the menu within 30 ms, awake when the race moves */
#include "tests/test.h"

#include "game/audio/rain_bus.h"

#include <stdlib.h>

enum { RATE = 0x5622 };   /* the mixer's (drive_audio.c) */

static void test_plain_below_the_knee(void)
{
    for (int mix = -RAIN_BUS_KNEE; mix <= RAIN_BUS_KNEE; mix += 97)
        for (int bus = -3000; bus <= 3000; bus += 251) {
            const int sum = mix + bus;
            if (abs(sum) <= RAIN_BUS_KNEE) CHECK_EQ_INT(rain_bus_sample((int16_t)mix, bus), sum);
        }
    CHECK_EQ_INT(rain_bus_sample(RAIN_BUS_KNEE - 100, 100), RAIN_BUS_KNEE);   /* the knee itself */
    CHECK_EQ_INT(rain_bus_sample(-RAIN_BUS_KNEE + 100, -100), -RAIN_BUS_KNEE);
}

/* above the knee: bounded by full scale, rising with the sum (no fold back), symmetric, and continuous at the knee */
static void test_bounded_above(void)
{
    int last = RAIN_BUS_KNEE;
    for (int32_t bus = 1; bus <= 200000; bus += 37) {
        const int y = rain_bus_sample(RAIN_BUS_KNEE, bus);
        CHECK(y >= last && y <= RAIN_BUS_FULL && y > RAIN_BUS_KNEE - 1);
        CHECK_EQ_INT(rain_bus_sample(-RAIN_BUS_KNEE, -bus), -y);
        last = y;
    }
    CHECK(last >= RAIN_BUS_FULL - 1);   /* far over: at full scale, not past it */
    CHECK(rain_bus_sample(RAIN_BUS_KNEE, 1) - RAIN_BUS_KNEE <= 1);   /* no step at the knee */
    CHECK(rain_bus_sample(32767, 32767) <= RAIN_BUS_FULL && rain_bus_sample(-32768, -32768) >= -RAIN_BUS_FULL);
    CHECK(rain_bus_sample(30000, 5000) < 32767);   /* a sum a little over full scale is rounded, not clipped flat */
}

/* where the bus is silent the race's own sample, whatever its level (Stock's and Restored's sound, and Restomod's without rain) */
static void test_only_where_the_rain_sounds(void)
{
    for (int mix = -32768; mix <= 32767; mix += 113) CHECK_EQ_INT(rain_bus_sample((int16_t)mix, 0), mix);
    CHECK_EQ_INT(rain_bus_sample(32767, 0), 32767);
    CHECK_EQ_INT(rain_bus_sample(-32768, 0), -32768);
    int16_t mix[6] = { 30000, -30000, 30000, 1000, -32768, 32767 };
    const int32_t still[6] = { 0, 0, 4000, 500, 0, -2000 }, moving[6] = { 0 };
    RainBus b = { 0 };
    rain_bus_meet(&b, mix, still, moving, 3, RATE);
    CHECK(mix[0] == 30000 && mix[1] == -30000 && mix[4] == -32768);   /* untouched where the bus is 0 */
    CHECK(mix[2] > RAIN_BUS_KNEE && mix[2] < 32767 && mix[3] == 1500);   /* over the knee rounded, under it the plain sum */
    CHECK_EQ_INT(mix[5], rain_bus_sample(32767, -2000));
}

/* the moving rain (the loops round a moving car, the sprays, the splashes) at half, -6 dB; the rain round a stopped car as it is */
static void test_the_moving_rain_at_half(void)
{
    int16_t mix[8] = { 0 };
    const int32_t still[8] = { 3000, -3000, 0, 0, 1000, 1000, 0, 0 }, moving[8] = { 0, 0, 6000, -6000, 2000, -2000, 1, -1 };
    RainBus b = { 0 };
    rain_bus_meet(&b, mix, still, moving, 4, RATE);
    CHECK(mix[0] == 3000 && mix[1] == -3000);   /* stopped: whole */
    CHECK(mix[2] == 3000 && mix[3] == -3000);   /* moving: half */
    CHECK(mix[4] == 2000 && mix[5] == 0);
    CHECK(mix[6] == 0 && mix[7] == 0);          /* a half of nothing is nothing */
    CHECK_EQ_INT(RAIN_BUS_MOVING_PERMILLE, 500);
}

/* `frames` frames of a steady bus (both inputs) over a silent mix, in blocks as the device's thread mixes; the last frame's left sample */
static int run(RainBus *b, int frames, int32_t level, int *first_silent)
{
    int16_t mix[2 * 64];
    int32_t still[2 * 64], moving[2 * 64];
    int last = 0;
    for (int done = 0; done < frames;) {
        const int n = frames - done < 64 ? frames - done : 64;
        for (int i = 0; i < 2 * n; i++) mix[i] = 0, still[i] = level, moving[i] = 2 * level;
        rain_bus_meet(b, mix, still, moving, (uint32_t)n, RATE);
        for (int i = 0; i < n; i++)
            if (first_silent != NULL && *first_silent < 0 && mix[2 * i] == 0 && mix[2 * i + 1] == 0) *first_silent = done + i;
        last = mix[2 * (n - 1)];
        done += n;
    }
    return last;
}

/* the pause or the menu: the rain silent within 30 ms with no step (each frame a little lower), silent while the race stands still, the race's mix untouched under it */
static void test_the_hush_is_quick_and_smooth(void)
{
    RainBus b = { 0 };
    CHECK_EQ_INT(run(&b, 100, 4000, NULL), 8000);   /* heard whole: 4000 + 8000 / 2 */
    CHECK(!rain_bus_silent(&b));
    rain_bus_hush(&b, 77);
    int16_t mix[2 * 2048];
    int32_t still[2 * 2048], moving[2 * 2048];
    for (int i = 0; i < 2 * 2048; i++) mix[i] = 0, still[i] = 4000, moving[i] = 8000;
    rain_bus_meet(&b, mix, still, moving, 2048, RATE);
    int silent_at = -1;
    for (int i = 0; i < 2048; i++) {
        if (i > 0) CHECK(mix[2 * i] <= mix[2 * (i - 1)] && mix[2 * (i - 1)] - mix[2 * i] <= 8000 / 300);   /* down, no click */
        if (silent_at < 0 && mix[2 * i] == 0) silent_at = i;
    }
    CHECK(silent_at > RATE * 10 / 1000 && silent_at <= RATE * 30 / 1000);   /* a fade, and done within 30 ms */
    CHECK(rain_bus_silent(&b));
    CHECK_EQ_INT(run(&b, RATE, 20000, NULL), 0);   /* a second later still nothing */
    int16_t race[4] = { 12345, -23456, 32767, -32768 };
    const int32_t loud[4] = { 30000, 30000, 30000, 30000 };
    rain_bus_meet(&b, race, loud, loud, 2, RATE);
    CHECK(race[0] == 12345 && race[1] == -23456 && race[2] == 32767 && race[3] == -32768);   /* the race's own sound as it is */
}

/* the race moving again wakes it (not a frame drawn while the clock stands still: the menu's own); the rain back as quickly and as smoothly */
static void test_the_rain_wakes_with_the_race(void)
{
    RainBus b = { 0 };
    CHECK(!rain_bus_wake(&b, 5));   /* never hushed: nothing to wake */
    rain_bus_hush(&b, 500);
    run(&b, RATE / 10, 4000, NULL);
    CHECK(!rain_bus_wake(&b, 500) && rain_bus_silent(&b));   /* the same tick: the pause, the menu, a pause then the menu */
    rain_bus_hush(&b, 500);
    CHECK(!rain_bus_wake(&b, 500));
    CHECK(rain_bus_wake(&b, 501) && !rain_bus_silent(&b));
    CHECK(!rain_bus_wake(&b, 502));   /* once */
    CHECK(run(&b, RATE * 30 / 1000, 4000, NULL) == 8000);   /* whole again within 30 ms */
    rain_bus_hush(&b, 900);
    CHECK(rain_bus_wake(&b, 10));   /* a clock gone back (a replay, a new race) moves too */
}

int main(void)
{
    test_plain_below_the_knee();
    test_bounded_above();
    test_only_where_the_rain_sounds();
    test_the_moving_rain_at_half();
    test_the_hush_is_quick_and_smooth();
    test_the_rain_wakes_with_the_race();
    return TEST_REPORT();
}
