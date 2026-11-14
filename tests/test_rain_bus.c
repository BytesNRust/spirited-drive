/* where the rain's bus meets the race's mix (rain_bus.h): the plain sum below the knee, rounded softly above it, never past full scale, and only where the rain sounds; the moving
 * rain at half; the weather's hush on all of it (hush.h) */
#include "tests/test.h"

#include "game/audio/hush.h"
#include "game/audio/rain_bus.h"

#include <stdlib.h>


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
    const int32_t whole[3] = { HUSH_UNITY, HUSH_UNITY, HUSH_UNITY };
    rain_bus_meet(mix, still, moving, whole, 3);
    CHECK(mix[0] == 30000 && mix[1] == -30000 && mix[4] == -32768);   /* untouched where the bus is 0 */
    CHECK(mix[2] > RAIN_BUS_KNEE && mix[2] < 32767 && mix[3] == 1500);   /* over the knee rounded, under it the plain sum */
    CHECK_EQ_INT(mix[5], rain_bus_sample(32767, -2000));
}

/* the moving rain (the loops round a moving car, the sprays, the splashes) at half, -6 dB; the rain round a stopped car as it is */
static void test_the_moving_rain_at_half(void)
{
    int16_t mix[8] = { 0 };
    const int32_t still[8] = { 3000, -3000, 0, 0, 1000, 1000, 0, 0 }, moving[8] = { 0, 0, 6000, -6000, 2000, -2000, 1, -1 };
    const int32_t whole[4] = { HUSH_UNITY, HUSH_UNITY, HUSH_UNITY, HUSH_UNITY };
    rain_bus_meet(mix, still, moving, whole, 4);
    CHECK(mix[0] == 3000 && mix[1] == -3000);   /* stopped: whole */
    CHECK(mix[2] == 3000 && mix[3] == -3000);   /* moving: half */
    CHECK(mix[4] == 2000 && mix[5] == 0);
    CHECK(mix[6] == 0 && mix[7] == 0);          /* a half of nothing is nothing */
    CHECK_EQ_INT(RAIN_BUS_MOVING_PERMILLE, 500);
}

/* the hush's gain (hush.h) on the whole bus, both inputs: whole as it is, half at half, none and the race's mix untouched */
static void test_the_hush_gain(void)
{
    const int32_t still[4] = { 4000, -4000, 30000, 30000 }, moving[4] = { 8000, -8000, 30000, 30000 };
    const int32_t whole[2] = { HUSH_UNITY, HUSH_UNITY }, half[2] = { HUSH_UNITY / 2, HUSH_UNITY / 2 }, none[2] = { 0, 0 };
    int16_t mix[4] = { 0, 0, 12345, -23456 };
    rain_bus_meet(mix, still, moving, whole, 1);
    CHECK(mix[0] == 8000 && mix[1] == -8000);
    int16_t m2[4] = { 0, 0, 0, 0 };
    rain_bus_meet(m2, still, moving, half, 1);
    CHECK(m2[0] == 4000 && m2[1] == -4000);
    int16_t m3[4] = { 0, 0, 12345, -23456 };
    rain_bus_meet(m3, still, moving, none, 2);
    CHECK(m3[0] == 0 && m3[1] == 0 && m3[2] == 12345 && m3[3] == -23456);
}

int main(void)
{
    test_plain_below_the_knee();
    test_bounded_above();
    test_only_where_the_rain_sounds();
    test_the_moving_rain_at_half();
    test_the_hush_gain();
    return TEST_REPORT();
}
