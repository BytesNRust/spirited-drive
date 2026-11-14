/* where the rain's bus meets the race's mix (rain_bus.h): the plain sum below the knee, rounded softly above it, never past full scale, and only where the rain sounds */
#include "tests/test.h"

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
    const int32_t bus[6] = { 0, 0, 4000, 500, 0, -2000 };
    rain_bus_meet(mix, bus, 3);
    CHECK(mix[0] == 30000 && mix[1] == -30000 && mix[4] == -32768);   /* untouched where the bus is 0 */
    CHECK(mix[2] > RAIN_BUS_KNEE && mix[2] < 32767 && mix[3] == 1500);   /* over the knee rounded, under it the plain sum */
    CHECK_EQ_INT(mix[5], rain_bus_sample(32767, -2000));
}

int main(void)
{
    test_plain_below_the_knee();
    test_bounded_above();
    test_only_where_the_rain_sounds();
    return TEST_REPORT();
}
