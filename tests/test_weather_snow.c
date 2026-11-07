/* the snow of a snowy track (weather_snow.h): what falls by weather and track, the flakes' count, fall and drift */
#include "tests/test.h"

#include "game/race/weather.h"
#include "game/race/weather_snow.h"

#include <stdlib.h>

static void test_level(void)
{
    CHECK_EQ_INT(weather_snow_level(WEATHER_RAIN, 1, 1), SNOW_LIGHT);
    CHECK_EQ_INT(weather_snow_level(WEATHER_STORM, 1, 1), SNOW_HEAVY);
    CHECK_EQ_INT(weather_snow_level(WEATHER_CLEAR, 1, 1), SNOW_NONE);   /* nothing falls in clear weather or fog */
    CHECK_EQ_INT(weather_snow_level(WEATHER_FOG, 1, 1), SNOW_NONE);
    for (int w = 0; w < WEATHER_COUNT; w++) {
        CHECK_EQ_INT(weather_snow_level(w, 0, 1), SNOW_NONE);   /* a track without `snow 1`: as it was */
        CHECK_EQ_INT(weather_snow_level(w, 1, 0), SNOW_NONE);   /* the switch off (Stock, Restored): as it was */
    }
    CHECK_EQ_INT(weather_snow_level(-1, 1, 1), SNOW_NONE);
    CHECK_EQ_INT(weather_snow_level(WEATHER_COUNT, 1, 1), SNOW_NONE);
    CHECK(weather_wet(WEATHER_RAIN) && weather_wet(WEATHER_STORM));   /* the road stays wet under the snow: weather_wet is unchanged */
}

static void test_count(void)
{
    const int light = weather_snow_count(640, 480, SNOW_LIGHT), heavy = weather_snow_count(640, 480, SNOW_HEAVY);
    CHECK_EQ_INT(light, SNOW_PER_640x480);
    CHECK_EQ_INT(heavy, SNOW_PER_640x480 * SNOW_HEAVY_TIMES);
    CHECK(weather_snow_count(1280, 960, SNOW_LIGHT) == 4 * light);   /* by the box's area */
    CHECK_EQ_INT(weather_snow_count(0, 480, SNOW_LIGHT), 0);
    CHECK_EQ_INT(weather_snow_count(640, 480, SNOW_NONE), 0);
    CHECK(weather_snow_count(8000, 6000, SNOW_HEAVY) <= SNOW_MAX);
    CHECK(weather_snow_count(1, 1, SNOW_LIGHT) >= 1);
}

static void test_flakes_stay_in_the_box_and_repeat(void)
{
    for (int level = SNOW_LIGHT; level <= SNOW_HEAVY; level++)
        for (uint32_t tick = 0; tick < 3000; tick += 37)
            for (int i = 0; i < weather_snow_count(640, 480, level); i += 5) {
                int32_t a[3], b[3];
                weather_snow_flake(i, tick, level, 640, 480, a);
                weather_snow_flake(i, tick, level, 640, 480, b);
                CHECK(a[0] == b[0] && a[1] == b[1] && a[2] == b[2]);   /* a pure function of its arguments */
                CHECK(a[0] >= 0 && a[0] < 640 && a[1] >= 0 && a[1] < 480);
                CHECK(a[2] >= 1 && a[2] <= SNOW_SIZE_MAX);
            }
}

/* the same flake `ticks` later: how far it has fallen (box heights a second, in percent) and drifted, over one second without a wrap */
static void test_fall_is_slow_and_drift_is_sideways(void)
{
    int slowest = 1000, fastest = 0, drifted = 0;
    for (int i = 0; i < 110; i++) {
        int32_t a[3], b[3];
        weather_snow_flake(i, 0, SNOW_LIGHT, 640, 480, a);
        weather_snow_flake(i, 30, SNOW_LIGHT, 640, 480, b);
        const int fell = ((b[1] - a[1]) + 480) % 480 * 100 / 480;   /* percent of the box's height in one second */
        if (fell < slowest) slowest = fell;
        if (fell > fastest) fastest = fell;
        if (abs(b[0] - a[0]) > 3 && abs(b[0] - a[0]) < 600) drifted++;
    }
    CHECK(slowest >= SNOW_FALL_MIN - 1 && fastest <= SNOW_FALL_MAX + 1);   /* never faster than the top: a rain streak crosses the box in about a second */
    CHECK(drifted > 55);                                                   /* most of them drift sideways, none straight down */
}

static void test_the_storm_leans_one_way(void)
{
    long lean = 0, still = 0;
    for (int i = 0; i < 300; i++) {
        int32_t a[3], b[3], c[3], d[3];
        weather_snow_flake(i, 0, SNOW_HEAVY, 640, 480, a), weather_snow_flake(i, 30, SNOW_HEAVY, 640, 480, b);
        weather_snow_flake(i, 0, SNOW_LIGHT, 640, 480, c), weather_snow_flake(i, 30, SNOW_LIGHT, 640, 480, d);
        const int dh = (b[0] - a[0] + 960) % 640, dl = (d[0] - c[0] + 960) % 640;   /* sideways over a second, 0..639 */
        lean += dh > 320 ? dh - 640 : dh;
        still += dl > 320 ? dl - 640 : dl;
    }
    CHECK(lean / 300 > 15 || lean / 300 < -15);                       /* the storm's wind: a mean push of the same sign */
    CHECK(labs(still / 300) < labs(lean / 300));                      /* the snow's own sway has no mean */
}

/* where the race's snow comes from: a replay's own record (whatever this machine has on), else the host's or the local setting (port_snow_weather) */
static void test_the_replay_has_the_say(void)
{
    CHECK_EQ_INT(snow_race_wanted(1, 1, 0), 1);   /* recorded on snow, local switch off: snow grip all the same */
    CHECK_EQ_INT(snow_race_wanted(1, 0, 1), 0);   /* recorded without, local switch on: as it was recorded */
    CHECK_EQ_INT(snow_race_wanted(0, 0, 1), 1);   /* a race, not a replay: the setting's */
    CHECK_EQ_INT(snow_race_wanted(0, 1, 0), 0);
    CHECK_EQ_INT(race_grip_nonstandard(0, 0), 0); /* records: kept for a dry race that is not on snow */
    CHECK_EQ_INT(race_grip_nonstandard(1, 0), 1); /* wet */
    CHECK_EQ_INT(race_grip_nonstandard(0, 1), 1); /* snow */
    CHECK_EQ_INT(race_grip_nonstandard(1, 1), 1);
}

int main(void)
{
    test_the_replay_has_the_say();
    test_level();
    test_count();
    test_flakes_stay_in_the_box_and_repeat();
    test_fall_is_slow_and_drift_is_sideways();
    test_the_storm_leans_one_way();
    return TEST_REPORT();
}
