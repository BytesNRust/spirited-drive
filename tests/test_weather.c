/* The weather's pure parts (weather.h): names, the rain's determinism and bounds. No game or CD data. */
#include "game/race/weather.h"
#include "test.h"

#include <string.h>

static void test_names(void)
{
    CHECK_EQ_INT(weather_named("clear"), WEATHER_CLEAR);
    CHECK_EQ_INT(weather_named("Fog"), WEATHER_FOG);
    CHECK_EQ_INT(weather_named("RAIN"), WEATHER_RAIN);
    CHECK_EQ_INT(weather_named("snow"), -1);
    CHECK_EQ_INT(weather_named(""), -1);
    CHECK_EQ_INT(weather_named("clearclearclear"), -1);
    for (int w = 0; w < WEATHER_COUNT; w++) CHECK_EQ_INT(weather_named(weather_name(w)), w);
    CHECK(strcmp(weather_name(99), "clear") == 0);
}

static void test_rain_is_a_function_of_drop_and_tick(void)
{
    int32_t a[4], b[4];
    weather_rain_streak(7, 123, 640, 480, a);
    weather_rain_streak(7, 123, 640, 480, b);
    CHECK(memcmp(a, b, sizeof a) == 0);
    weather_rain_streak(7, 124, 640, 480, b);
    CHECK(b[3] != a[3]);                              /* the tick moves it */
    weather_rain_streak(8, 123, 640, 480, b);
    CHECK(b[2] != a[2] || b[3] != a[3]);              /* the drops differ */
}

static void test_rain_bounds_and_scale(void)
{
    for (int drop = 0; drop < 300; drop++)
        for (uint32_t tick = 0; tick < 40; tick += 7) {
            int32_t s[4];
            weather_rain_streak(drop, tick, 640, 480, s);
            CHECK(s[3] >= 0 && s[3] < 480);                       /* the head is inside the box's rows */
            CHECK(s[1] < s[3]);                                   /* the tail is above it */
            CHECK(s[2] <= 640 && s[2] > -200);
        }
    int32_t one[4], two[4];
    weather_rain_streak(5, 77, 640, 480, one);
    weather_rain_streak(5, 77, 1280, 960, two);            /* the same drop in the same view at scale 2 */
    CHECK(two[3] == 2 * one[3] || two[3] == 2 * one[3] + 1 || two[3] == 2 * one[3] - 1);
    CHECK_EQ_INT(weather_rain_width(2), 2);
    CHECK_EQ_INT(weather_rain_width(0), 1);
    CHECK_EQ_INT(weather_rain_count(0, 480), 0);
    CHECK(weather_rain_count(1280, 960) == 4 * weather_rain_count(640, 480));
    CHECK(weather_rain_count(1, 1) >= 1 && weather_rain_count(100000, 100000) <= 2000);
}

static void test_sky_meets_the_fog_at_the_horizon(void)
{
    const float up[3] = { 0, 1, 0 };   /* level camera: the horizon is the row of the centre */
    for (int x = 0; x < 8; x++) {
        CHECK_EQ_INT(weather_sky_level(x, 240, 320, 240, 400.0f, up, 11, 15), 15);   /* at the horizon the sky is the fog */
        CHECK_EQ_INT(weather_sky_level(x, 400, 320, 240, 400.0f, up, 11, 15), 15);   /* below it too */
        CHECK_EQ_INT(weather_sky_level(x, 20, 320, 240, 400.0f, up, 11, 15), 11);    /* high up, its own level */
        CHECK_EQ_INT(weather_sky_level(x, 20, 320, 240, 400.0f, up, 8, 8), 8);       /* no gradient: the same everywhere */
    }
    int mid = 0;   /* between: a mean of the dither's levels, in between */
    for (int x = 0; x < 16; x++) mid += weather_sky_level(x, 200, 320, 240, 400.0f, up, 11, 15);
    CHECK(mid > 11 * 16 && mid < 15 * 16);
}

int main(void)
{
    test_names();
    test_rain_is_a_function_of_drop_and_tick();
    test_rain_bounds_and_scale();
    test_sky_meets_the_fog_at_the_horizon();
    return TEST_REPORT();
}
