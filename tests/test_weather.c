/* The weather's pure parts (weather.h): names, the rain's determinism and bounds. No game or CD data. */
#include "game/race/weather.h"
#include "game/race/headlight_pool.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_names(void)
{
    CHECK_EQ_INT(weather_named("clear"), WEATHER_CLEAR);
    CHECK_EQ_INT(weather_named("Fog"), WEATHER_FOG);
    CHECK_EQ_INT(weather_named("RAIN"), WEATHER_RAIN);
    CHECK_EQ_INT(weather_named("storm"), WEATHER_STORM);
    CHECK(weather_wet(WEATHER_RAIN) && weather_wet(WEATHER_STORM) && !weather_wet(WEATHER_FOG) && !weather_wet(WEATHER_CLEAR));
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
    int level, still_falls = 0, radial = 0, n = 0, wedge = 0, shown = 0;
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
        if (level >= 0 && dist > 4 && (f[3] - vy) / dist >= 0.8) wedge++;                /* the wedge straight down: some drops, not none, and not many */
        if (level >= 0) shown++;
    }
    CHECK(wedge > 0 && wedge * 100 < shown * 20);
    CHECK(still_falls > 230);
    CHECK(n > 150 && radial * 100 > n * 97);
    int32_t slow[4], mid[4];
    weather_rain_spoke(9, 50, 5 << 16, 640, 480, vx, vy, 500.0f, 0, slow, &level);   /* it changes smoothly with the speed */
    weather_rain_spoke(9, 50, (5 << 16) + 6554, 640, 480, vx, vy, 500.0f, 0, mid, &level);   /* a tenth of a metre a second more */
    CHECK(abs(slow[2] - mid[2]) + abs(slow[3] - mid[3]) < 40);
}

/* rain v3: a drop keeps its side of the vanishing point as the speed grows (the direction is turned per drop, none crosses over), and a wide view is wet to its right edge */
static void test_rain_keeps_its_side_and_fills_a_wide_view(void)
{
    const int32_t vx = 560, vy = 200;   /* a 21:9 box 1120 x 480 */
    int level, flips = 0, tested = 0, right = 0, seen = 0;
    for (int drop = 0; drop < 400; drop++) {
        int prev = 0;
        for (int m = 0; m <= 40; m += 2) {
            int32_t s[4];
            weather_rain_spoke(drop, 20, m << 16, 1120, 480, vx, vy, 500.0f, 0, s, &level);
            if (level < 0 || abs(s[2] - vx) < 60) continue;   /* gone, or at the vanishing point itself: no side */
            const int side = s[2] > vx ? 1 : -1;
            if (prev != 0 && side != prev) flips++;
            prev = side;
            tested++;
        }
        for (uint32_t tick = 0; tick < 40; tick += 3) {   /* standing: where the heads are */
            int32_t s[4];
            weather_rain_spoke(drop, tick, 0, 1120, 480, vx, vy, 500.0f, 0, s, &level);
            seen++;
            if (s[2] > 1120 * 85 / 100 && s[2] < 1120 && s[3] > 0 && s[3] < 480) right++;
        }
    }
    CHECK(tested > 2000 && flips * 100 < tested);   /* no drop crosses from one side to the other */
    CHECK(right * 100 > seen * 6);                  /* the right 15 % is not dry (it was, with columns over the box alone) */
}

/* rain v3: drops on the lens land, stay, slide a little and fade; a pure function, inside the box, not always there, never moving fast */
static void test_lens_drops(void)
{
    CHECK(weather_lens_count(640, 480) == WEATHER_LENS_PER_640x480 && weather_lens_count(1120, 480) > weather_lens_count(640, 480) && weather_lens_count(0, 480) == 0);
    int here = 0, total = 0, grew = 0, faded = 0;
    for (int drop = 0; drop < weather_lens_count(1120, 480); drop++) {
        int32_t prev[4] = { 0 };
        int had = 0;
        for (uint32_t tick = 0; tick < 600; tick++) {
            int32_t a[4], b[4];
            const int ia = weather_lens_drop(drop, tick, 0, 1120, 480, 560, 200, a), ib = weather_lens_drop(drop, tick, 0, 1120, 480, 560, 200, b);
            CHECK(ia == ib && (!ia || memcmp(a, b, sizeof a) == 0));   /* a pure function */
            total++;
            if (!ia) { had = 0; continue; }
            here++;
            CHECK(a[0] >= 0 && a[0] < 1120 && a[1] >= 0 && a[1] < 480 && a[2] >= 1 && a[2] <= 7 && a[3] >= 1 && a[3] <= 16);
            const int new_life = a[3] == 16 && prev[3] < 8 && a[2] <= prev[2] + 3;   /* a life that begins as the last one ended: its own place */
            if (had && !new_life) {
                CHECK(abs(a[0] - prev[0]) + abs(a[1] - prev[1]) <= 2);   /* it sticks: a pixel or two a frame at most */
                if (a[2] > prev[2]) grew++;
                if (a[3] < prev[3]) faded++;
            }
            memcpy(prev, a, sizeof prev), had = 1;
        }
    }
    CHECK(here * 100 > total * 25 && here * 100 < total * 85);   /* the glass is wet part of the time */
    CHECK(grew > 0 && faded > 0);                                 /* they land (grow) and go (fade) */
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

/* WEATHER_STORM: the storm's clock: a strike is full on its first tick and decays; one or two a slot, the second weaker and well apart */
static void test_storm_clock(void)
{
    int strikes = 0, doubles = 0, quiet_slots = 0;
    for (uint32_t t = 0; t < WEATHER_STORM_QUIET; t++) CHECK_EQ_INT(weather_rain_flash(t), 0);   /* none in the first 5 seconds */
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
    CHECK_EQ_INT(WEATHER_STORM_SLOT, 15 * 60);   /* a strike every 15 s, a flash a third of a second, none in the first 5 s (60 ticks a second) */
    CHECK_EQ_INT(WEATHER_FLASH_TICKS, 20);
    CHECK_EQ_INT(WEATHER_STORM_QUIET, 5 * 60);
    CHECK(WEATHER_THUNDER_MIN_TICKS == 30 && WEATHER_THUNDER_MAX_TICKS == 240);   /* 0.5 to 4 s */
    CHECK(strikes > 30 && strikes < 56);   /* three slots in four */
    CHECK(doubles > 2 && doubles * 2 < strikes);
    CHECK(quiet_slots > 2);
}

static void test_spokes_from_a_vanishing_point_off_the_screen(void)
{
    /* the outside cameras: the vanishing point (the projection of the car's velocity) may be off the box; the spokes then leave it nearly parallel, all inside the box, none through it */
    const int32_t vx = 2600, vy = 900;   /* far to the lower right of a 640 x 480 box */
    int inside = 0, shown = 0, parallel = 0;
    for (int drop = 0; drop < weather_rain_count_fx(640, 480); drop++)
        for (uint32_t tick = 0; tick < 40; tick += 7) {
            int32_t s[4];
            int level;
            weather_rain_spoke(drop, tick, 40 << 16, 640, 480, vx, vy, 500.0f, 0, s, &level);
            if (level < 0) continue;
            shown++;
            if (s[2] >= -2 && s[2] <= 642 && s[3] >= -2 && s[3] <= 482) inside++;
            const float dx = (float)(s[2] - s[0]), dy = (float)(s[3] - s[1]), n = sqrtf(dx * dx + dy * dy);
            if (n > 6.0f && dx / n < -0.5f && dy / n < -0.1f) parallel++;   /* they flow away from the point: toward the upper left */
        }
    CHECK(shown > 100);
    CHECK(inside * 100 >= shown * 95);     /* the heads stay in the view (the few that begin as a place just outside the box, for the wide views, are clipped by the drawing) */
    CHECK(parallel * 100 >= shown * 85);   /* and stream away from the point */
}

static void test_reflection_weight(void)
{
    CHECK_EQ_INT(weather_reflection_weight(1.0f, 10.0f), 100);     /* a car facing the camera, near: full */
    CHECK_EQ_INT(weather_reflection_weight(-1.0f, 10.0f), 100);    /* the rear: the same weight (the caller picks the lamp) */
    CHECK_EQ_INT(weather_reflection_weight(0.2f, 10.0f), 0);       /* a side view: none */
    CHECK_EQ_INT(weather_reflection_weight(-0.29f, 10.0f), 0);
    CHECK_EQ_INT(weather_reflection_weight(0.5f, 10.0f), 50);      /* half way between 0.3 and 0.7 */
    CHECK_EQ_INT(weather_reflection_weight(0.8f, 25.0f), 100);     /* full to 25 m */
    CHECK_EQ_INT(weather_reflection_weight(0.8f, 50.0f), 50);      /* half way to 75 m */
    CHECK_EQ_INT(weather_reflection_weight(0.8f, 75.0f), 0);       /* gone by 75 m */
    CHECK_EQ_INT(weather_reflection_weight(0.8f, 150.0f), 0);
    int last = 101;
    for (float d = 20.0f; d <= 80.0f; d += 2.0f) {                  /* the fade only falls with distance */
        const int w = weather_reflection_weight(0.9f, d);
        CHECK(w <= last);
        last = w;
    }
}

static void test_rain_v2(void)
{
    /* the counts: v2 0 as before (rain x1, storm x3), v2 1 half and a quarter over the old rain */
    CHECK_EQ_INT(weather_rain_scaled(200, 0, 0), 200);
    CHECK_EQ_INT(weather_rain_scaled(200, 1, 0), 200 * WEATHER_STORM_DROPS);
    CHECK_EQ_INT(weather_rain_scaled(200, 0, 1), 100);
    CHECK_EQ_INT(weather_rain_scaled(200, 1, 1), 250);
    CHECK_EQ_INT(weather_rain_scaled(1, 0, 1), 1);   /* never none */
    CHECK_EQ_INT(weather_rain_scaled(0, 0, 1), 0);
    /* the thunder: a strike is a flash that rises, once per flash, a delay of 0.5..4 s that is the tick's alone, quieter the later */
    int strikes = 0;
    for (uint32_t t = 0; t < 20 * WEATHER_STORM_SLOT; t++) {
        const int s = weather_thunder_strike(t);
        CHECK_EQ_INT(s, weather_thunder_strike(t));
        if (!s) continue;
        strikes++;
        CHECK(weather_rain_flash(t) > 0);
        const int d = weather_thunder_delay(t);
        CHECK(d >= WEATHER_THUNDER_MIN_TICKS && d <= WEATHER_THUNDER_MAX_TICKS);
        CHECK_EQ_INT(d, weather_thunder_delay(t));
        const int v = weather_thunder_volume(d);
        CHECK(v >= WEATHER_THUNDER_FAR_PERCENT && v <= 100);
    }
    CHECK(strikes >= 10);
    CHECK_EQ_INT(weather_thunder_strike(0), 0);
    CHECK_EQ_INT(weather_thunder_volume(WEATHER_THUNDER_MIN_TICKS), 100);
    CHECK_EQ_INT(weather_thunder_volume(WEATHER_THUNDER_MAX_TICKS), WEATHER_THUNDER_FAR_PERCENT);
    CHECK(weather_thunder_volume(60) > weather_thunder_volume(90));   /* later, quieter */
    /* the lamps' reflections stay on the carriageway: a verge of 40 eighths is 5 m */
    CHECK_EQ_INT(weather_on_carriageway(-2 * 65536, 40, 40), 1);        /* the left lane */
    CHECK_EQ_INT(weather_on_carriageway(4 * 65536, 40, 40), 1);         /* the right lane */
    CHECK_EQ_INT(weather_on_carriageway(5 * 65536, 40, 40), 1);         /* the line itself */
    CHECK_EQ_INT(weather_on_carriageway(6 * 65536, 40, 40), 0);         /* the right shoulder */
    CHECK_EQ_INT(weather_on_carriageway(-6 * 65536, 40, 40), 0);        /* the left shoulder */
    CHECK_EQ_INT(weather_on_carriageway(-6 * 65536, 56, 40), 1);        /* a wider left side (a third lane): its own verge */
    CHECK_EQ_INT(weather_on_carriageway(7 * 65536, 56, 40), 0);         /* but the right is still 5 m */
    CHECK_EQ_INT(weather_on_carriageway(9 * 65536, 0, 0), 1);           /* no verge known: as before */
    /* the sideways distance of a ground point from a node's centre line (car_walls' `across`: negative left, positive right): the road runs along (sin a, cos a)
     * in (x, z), so it is the point's offset's share across that, not along it (car_lateral's is the along-road one, and a check made on it let the shoulder through) */
    CHECK_EQ_INT(weather_across_road(0, 3 * 65536, 40 * 65536), 3 * 65536);         /* heading +z: 3 m to +x, 40 m on along: 3 m across */
    CHECK_EQ_INT(weather_across_road(0, -3 * 65536, -40 * 65536), -3 * 65536);
    CHECK_EQ_INT(weather_across_road(0, 0, 9 * 65536), 0);                          /* along the road only: on its centre line */
    CHECK_EQ_INT(weather_across_road(0x4000, 40 * 65536, -3 * 65536), 3 * 65536);   /* heading +x: 3 m to -z is the same side */
    CHECK_EQ_INT(weather_across_road(0x4000, 7 * 65536, 0), 0);
    CHECK(abs(weather_across_road(0x2000, 4 * 65536, 4 * 65536)) < 64);             /* heading diagonal: the diagonal is along */
    /* a lamp 6 m across on a 5 m verge is off the carriageway wherever along the node it is; 2 m across never is */
    CHECK_EQ_INT(weather_on_carriageway(weather_across_road(0, 6 * 65536, 3 * 65536), 40, 40), 0);
    CHECK_EQ_INT(weather_on_carriageway(weather_across_road(0, 2 * 65536, 12 * 65536), 40, 40), 1);
    int low = 0, high = 0;   /* the delays use the whole range */
    for (uint32_t t = 150; t < 50000; t++) {
        const int d = weather_thunder_delay(t);
        low |= d < 40, high |= d > 95;
    }
    CHECK(low && high);
}

/* port (weather_zones): the weather at the eye follows the closed zones */
static void test_weather_zones(void)
{
    CHECK_EQ_INT(weather_open_for_distance(-1), 16);   /* no closed node near */
    CHECK_EQ_INT(weather_open_for_distance(WEATHER_MOUTH_NODES), 16);
    CHECK_EQ_INT(weather_open_for_distance(0), 0);     /* inside */
    CHECK_EQ_INT(weather_open_for_distance(3), 8);     /* the fade over the mouth */
    int last = -1;
    for (int d = 0; d <= WEATHER_MOUTH_NODES; d++) {
        const int o = weather_open_for_distance(d);
        CHECK(o >= last);
        last = o;
    }
    CHECK_EQ_INT(weather_flash_open(15, 16), 15);      /* in the open the lightning is whole */
    CHECK_EQ_INT(weather_flash_open(15, 0), 0);        /* inside there is none */
    CHECK_EQ_INT(weather_flash_open(14, 8), 7);
    CHECK_EQ_INT(weather_thunder_inside(100, 16), 100);
    CHECK_EQ_INT(weather_thunder_inside(100, 0), WEATHER_THUNDER_INSIDE_PERCENT);   /* muffled, not silent */
    CHECK_EQ_INT(weather_thunder_inside(0, 0), 0);
    float end = 4200.0f, cap = 0.93f;
    CHECK_EQ_INT(weather_fog_open(16, 250.0f, &end, &cap), 1);   /* the open: untouched */
    CHECK(end == 4200.0f && cap == 0.93f);
    CHECK_EQ_INT(weather_fog_open(0, 250.0f, &end, &cap), 0);    /* inside: no fog */
    CHECK(end == 4200.0f);
    CHECK_EQ_INT(weather_fog_open(8, 250.0f, &end, &cap), 1);    /* half way: the same fog twice as far, half as strong */
    CHECK(end == 250.0f + 3950.0f * 2.0f);
    CHECK(cap > 0.46f && cap < 0.47f);
}

int main(void)
{
    test_weather_zones();
    test_reflection_weight();
    test_storm_clock();
    test_rain_spray_plume();
    test_rain_spokes_follow_the_speed();
    test_rain_keeps_its_side_and_fills_a_wide_view();
    test_lens_drops();
    test_spokes_from_a_vanishing_point_off_the_screen();
    test_rain_cone_lights_the_drops();
    test_fog_curve_default();
    test_fog_for_tint();
    test_names();
    test_rain_is_a_function_of_drop_and_tick();
    test_rain_bounds_and_scale();
    test_sky_meets_the_fog_at_the_horizon();
    test_rain_v2();
    return TEST_REPORT();
}
