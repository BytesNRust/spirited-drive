/* The weather's pure parts (weather.h): names, the rain's determinism and bounds. No game or CD data. */
#include "game/race/weather.h"
#include "game/race/headlight_pool.h"
#include "test.h"

#include <math.h>
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
    CHECK(weather_rain_count_fx(640, 480) * 10 < weather_rain_count(640, 480) * 7 && weather_rain_count_fx(640, 480) > 0);   /* the speed-following rain is sparser */
    CHECK(weather_rain_blend(0) == 0.0f && weather_rain_blend(WEATHER_SPOKE_TURNED << 16) == 1.0f && weather_rain_blend(6 << 16) > 0.0f);
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
    int level, still_falls = 0, radial = 0, n = 0;
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
        CHECK(span2(f) <= 80 * 80);                                                      /* short: no more than a sixth of the view */
        const double dist = sqrt((double)(f[2] - vx) * (f[2] - vx) + (double)(f[3] - vy) * (f[3] - vy));
        if (dist > 4) CHECK((f[3] - vy) / dist < 0.6);                                   /* never in the wedge straight down: the road ahead */
    }
    CHECK(still_falls > 230);
    CHECK(n > 150 && radial * 100 > n * 97);
    int32_t slow[4], mid[4];
    weather_rain_spoke(9, 50, 5 << 16, 640, 480, vx, vy, 500.0f, 0, slow, &level);   /* it changes smoothly with the speed */
    weather_rain_spoke(9, 50, (5 << 16) + 6554, 640, 480, vx, vy, 500.0f, 0, mid, &level);   /* a tenth of a metre a second more */
    CHECK(abs(slow[2] - mid[2]) + abs(slow[3] - mid[3]) < 40);
}

static void test_rain_cone_lights_the_drops(void)
{
    CHECK(weather_rain_cone_weight(15.0f, 0.0f, 1.0f) > 200);   /* ahead of the nose */
    CHECK_EQ_INT(weather_rain_cone_weight(15.0f, 40.0f, 1.0f), 0);   /* far to the side */
    CHECK_EQ_INT(weather_rain_cone_weight(-25.0f, 0.0f, 1.0f), 0);    /* well behind: past the glow round the car */
    CHECK_EQ_INT(weather_rain_cone_weight(15.0f, 0.0f, 20.0f), 0);    /* high above */
    CHECK_EQ_INT(weather_rain_cone_weight(150.0f, 0.0f, 1.0f), 0);    /* beyond the reach */
    CHECK_EQ_INT(weather_rain_cone_weight(8.0f, 3.0f, 1.0f), weather_rain_cone_weight(8.0f, -3.0f, 1.0f));   /* symmetric */
    /* it is the cone's own weight, the eye half a metre ahead of the car's centre */
    CHECK_EQ_INT(weather_rain_cone_weight(15.0f, 4.0f, 2.0f), headlight_cone_weight_local(15.5, 4.0, 2.0));
    CHECK(weather_rain_cone_weight(30.0f, 0.0f, 1.0f) < weather_rain_cone_weight(15.0f, 0.0f, 1.0f));   /* it fades with the distance */
    int dark = 0, bright = 0, level;
    for (int drop = 0; drop < 240; drop++) {
        int32_t s[4];
        weather_rain_spoke(drop, 7, 0, 640, 480, 320, 200, 500.0f, 1, s, &level);
        CHECK(level >= 0 && level < WEATHER_RAIN_LEVELS);
        if (level == 0) dark++;
        if (level >= 2) bright++;
    }
    CHECK(dark > 0 && bright > dark);   /* the cone is a broad patch: most drops are in it, a few outside */
    CHECK(weather_rain_level_fog(0) < weather_rain_level_fog(1) && weather_rain_level_fog(2) < weather_rain_level_fog(3));
}

/* rain_fx: the spray's plume */
static void test_rain_spray_plume(void)
{
    int32_t rel[3], size, far_slow = 0, far_fast = 0;
    CHECK_EQ_INT(weather_rain_spray_puff(3, 1, 10, 5 << 16, 1, rel, &size), 0);    /* under 8 m/s: none */
    CHECK_EQ_INT(size, 0);
    CHECK_EQ_INT(weather_rain_spray_puff(3, 1, 10, 30 << 16, 0, rel, &size), 0);   /* in the air or on the shoulder: none */
    CHECK_EQ_INT(weather_rain_spray_puff(WEATHER_SPRAY_PUFFS, 1, 10, 30 << 16, 1, rel, &size), 0);
    int live = 0;
    for (int k = 0; k < WEATHER_SPRAY_PUFFS; k++) {
        int32_t a[3], b[3], sa, sb;
        const int ra = weather_rain_spray_puff(k, -1, 77, 30 << 16, 1, a, &sa), rb = weather_rain_spray_puff(k, -1, 77, 30 << 16, 1, b, &sb);
        CHECK(ra == rb && sa == sb && memcmp(a, b, sizeof a) == 0);                  /* a pure function */
        if (ra) live++;
        CHECK(a[0] > 0);                                                            /* behind the wheel */
        CHECK(a[1] < 0);                                                            /* the left wheel's puffs go out to the left */
        int32_t s2[3], z2;
        weather_rain_spray_puff(k, -1, 77, 15 << 16, 1, s2, &z2);
        if (k == WEATHER_SPRAY_PUFFS - 1) far_slow = s2[0], far_fast = a[0];
    }
    CHECK(live >= WEATHER_SPRAY_PUFFS - 2);
    CHECK(far_fast > far_slow);                                                     /* the plume is longer the faster */
    int32_t r0[3], r1[3], r2[3], z0, z1, z2;                                        /* a puff grows as it goes from the wheel, up to a cap */
    weather_rain_spray_puff(1, 1, 5, 30 << 16, 1, r0, &z0);
    weather_rain_spray_puff(10, 1, 5, 30 << 16, 1, r1, &z1);
    CHECK(z1 > z0);
    int biggest = 0;
    for (int k = 0; k < WEATHER_SPRAY_PUFFS; k++)
        for (int side = -1; side <= 1; side += 2) {
            weather_rain_spray_puff(k, side, 9, 45 << 16, 1, r2, &z2);
            if (z2 > biggest) biggest = z2;
        }
    CHECK(biggest <= (int)(256.0f * WEATHER_SPRAY_CAP) && biggest > 256);
    weather_rain_spray_puff(WEATHER_SPRAY_PUFFS - 1, 1, 5, 30 << 16, 1, r0, &z0);   /* and the trail's end is not thinner than a puff near the wheel */
    CHECK(z0 > 0);
}

/* rain_lightning: the storm's clock: a strike is full on its first tick and decays; one or two a slot, the second weaker and well apart */
static void test_storm_clock(void)
{
    int strikes = 0, doubles = 0, quiet_slots = 0;
    for (uint32_t t = 0; t < 150; t++) CHECK_EQ_INT(weather_rain_flash(t), 0);   /* none in the first 5 seconds */
    for (uint32_t slot = 0; slot < 60; slot++) {
        int lit = 0, peaks = 0, last = 0;
        for (uint32_t t = slot * WEATHER_STORM_SLOT; t < (slot + 1) * WEATHER_STORM_SLOT; t++) {
            const int f = weather_rain_flash(t);
            CHECK_EQ_INT(f, weather_rain_flash(t));                                   /* a pure function of the tick */
            CHECK(f >= 0 && f <= WEATHER_FLASH_PEAK);
            if (f) lit++;
            if (f > last && last == 0) peaks++;                                       /* a rise from dark: a flash begins */
            if (f > 0 && last > 0) CHECK(f <= last);                                  /* then it only decays */
            last = f;
        }
        CHECK(peaks <= 2);                                                            /* one or two flashes a strike */
        if (slot > 0 && lit == 0) quiet_slots++;
        if (peaks == 2) doubles++;
        if (peaks > 0) strikes++;
        if (peaks > 0) {
            int top = 0;
            for (uint32_t t = slot * WEATHER_STORM_SLOT; t < (slot + 1) * WEATHER_STORM_SLOT; t++) top = weather_rain_flash(t) > top ? weather_rain_flash(t) : top;
            CHECK_EQ_INT(top, WEATHER_FLASH_PEAK);                                    /* the sky goes white at the strike */
        }
    }
    CHECK(strikes > 30 && strikes < 56);   /* three slots in four */
    CHECK(doubles > 2 && doubles * 2 < strikes);
    CHECK(quiet_slots > 2);
}

int main(void)
{
    test_storm_clock();
    test_rain_spray_plume();
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
