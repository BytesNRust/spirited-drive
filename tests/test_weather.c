/* The weather's pure parts (weather.h): names, the rain's determinism and bounds. No game or CD data. */
#include "game/race/weather.h"
#include "test.h"

#include <stdlib.h>
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

/* the fog for a tinted palette: after the tint it is the day colour with half the tint; neutral changes nothing; clamped */
static void test_fog_curve_default(void)
{
    unsetenv("TNFS_FOG_CURVE");
    CHECK_EQ_INT(weather_fog_curve(), 1);   /* the exponential is the default */
    setenv("TNFS_FOG_CURVE", "line", 1);
    CHECK_EQ_INT(weather_fog_curve(), 0);
    setenv("TNFS_FOG_CURVE", "exp", 1);
    CHECK_EQ_INT(weather_fog_curve(), 1);
    unsetenv("TNFS_FOG_CURVE");
}

static void test_fog_for_tint(void)
{
    const uint8_t fog[3] = { 100, 120, 140 };
    uint8_t out[3];
    const float neutral[3] = { 1, 1, 1 };
    weather_fog_for_tint(fog, neutral, out);
    CHECK(out[0] == 100 && out[1] == 120 && out[2] == 140);
    const float tint[3] = { 0.8f, 0.6f, 0.5f };
    weather_fog_for_tint(fog, tint, out);
    for (int k = 0; k < 3; k++) {
        const float shown = (float)out[k] * tint[k], half = (float)fog[k] * (1.0f + tint[k]) / 2.0f;   /* after the palette's tint */
        CHECK(shown > half - 1.5f && shown < half + 1.5f);
    }
    const uint8_t bright[3] = { 250, 250, 250 };
    const float dark[3] = { 0.1f, 0.1f, 0.1f };
    weather_fog_for_tint(bright, dark, out);
    CHECK(out[0] == 255 && out[1] == 255 && out[2] == 255);
    weather_fog_for_tint(fog, tint, out);   /* in place too */
    uint8_t again[3] = { 100, 120, 140 };
    weather_fog_for_tint(again, tint, again);
    CHECK(memcmp(out, again, 3) == 0);
}

static int span2(const int32_t s[4]) { return (s[2] - s[0]) * (s[2] - s[0]) + (s[3] - s[1]) * (s[3] - s[1]); }

/* rain_fx: a standing car's streak falls; a fast car's is a spoke away from the vanishing point, longer; a pure function; lit by the cone at night */
static void test_rain_spokes_follow_the_speed(void)
{
    const int32_t vx = 320, vy = 200;
    int level, still_falls = 0, radial = 0, longer = 0, n = 0;
    for (int drop = 0; drop < 240; drop++) {
        int32_t a[4], f[4], g[4];
        weather_rain_spoke(drop, 50, 0, 640, 480, vx, vy, 500.0f, 0, a, &level);
        CHECK_EQ_INT(level, WEATHER_RAIN_DAY_LEVEL);                                    /* by day the phase 1 colour */
        weather_rain_spoke(drop, 50, 40 << 16, 640, 480, vx, vy, 500.0f, 0, f, &level);
        weather_rain_spoke(drop, 50, 40 << 16, 640, 480, vx, vy, 500.0f, 0, g, &level);
        CHECK(memcmp(f, g, sizeof f) == 0);                                              /* a pure function */
        if (a[3] > a[1]) still_falls++;                                                  /* the head is the lower end */
        if (span2(f) < 16) continue;
        n++;
        const int64_t outward = (int64_t)(f[2] - vx) * (f[2] - f[0]) + (int64_t)(f[3] - vy) * (f[3] - f[1]);   /* the head is farther out than the tail */
        if (outward > 0) radial++;
        if (span2(f) > span2(a)) longer++;
    }
    CHECK(still_falls > 230);
    CHECK(n > 200 && radial * 100 > n * 97);
    CHECK(longer * 100 > 240 * 55);   /* most are longer (the ones just born are cut by the origin) */
    int32_t slow[4], mid[4];
    weather_rain_spoke(9, 50, 5 << 16, 640, 480, vx, vy, 500.0f, 0, slow, &level);   /* it changes smoothly with the speed */
    weather_rain_spoke(9, 50, 6 << 16, 640, 480, vx, vy, 500.0f, 0, mid, &level);
    CHECK(abs(slow[2] - mid[2]) + abs(slow[3] - mid[3]) < 40);
}

static void test_rain_cone_lights_the_drops(void)
{
    CHECK(weather_rain_cone_weight(15.0f, 0.0f, 1.0f) > 200);   /* ahead of the nose */
    CHECK_EQ_INT(weather_rain_cone_weight(15.0f, 12.0f, 1.0f), 0);   /* far to the side */
    CHECK_EQ_INT(weather_rain_cone_weight(-5.0f, 0.0f, 1.0f), 0);    /* behind */
    CHECK_EQ_INT(weather_rain_cone_weight(15.0f, 0.0f, 6.0f), 0);    /* high above */
    int dark = 0, bright = 0, level;
    for (int drop = 0; drop < 240; drop++) {
        int32_t s[4];
        weather_rain_spoke(drop, 7, 0, 640, 480, 320, 200, 500.0f, 1, s, &level);
        CHECK(level >= 0 && level < WEATHER_RAIN_LEVELS);
        if (level == 0) dark++;
        if (level >= 2) bright++;
    }
    CHECK(dark > 0 && bright > 0 && dark > bright);   /* a few in the beam, most outside it */
    CHECK(weather_rain_level_fog(0) < weather_rain_level_fog(1) && weather_rain_level_fog(2) < weather_rain_level_fog(3));
}

int main(void)
{
    test_rain_spokes_follow_the_speed();
    test_rain_cone_lights_the_drops();
    test_fog_curve_default();
    test_fog_for_tint();
    test_names();
    test_rain_is_a_function_of_drop_and_tick();
    test_rain_bounds_and_scale();
    test_sky_meets_the_fog_at_the_horizon();
    return TEST_REPORT();
}
