/* The windscreen wipers' pure parts (wiper.h): the sweep's timing and its fraction, the blade's angle, the sector a pass clears and when it last passed a point. */
#include "game/race/wiper.h"
#include "game/race/weather.h"
#include "tests/test.h"

#include <math.h>

static WiperBlade blade(void)
{
    const WiperBlade b = { 300.0f, 400.0f, 200.0f, 3.14159265f * 0.9f, 3.14159265f * 0.4f, 0, 0, 0, 0, 0 };   /* parked pointing left, sweeping up to near vertical */
    return b;
}

static void test_the_sweep_is_a_triangle_and_parks(void)
{
    const WiperTiming t = wiper_timing(WIPER_RAIN);
    CHECK(t.period > 0 && t.sweep > 0 && t.sweep <= t.period);
    CHECK_EQ_INT(t.period, 108);                                         /* the rain's 1.8 s, light 2.4 s, storm 1.3 s, snow as the rain (60 ticks a second) */
    CHECK_EQ_INT(wiper_timing(WIPER_LIGHT).period, 144);
    CHECK_EQ_INT(wiper_timing(WIPER_STORM).period, 78);
    CHECK_EQ_INT(wiper_timing(WIPER_SNOW).period, 108);
    CHECK(wiper_timing(WIPER_STORM).period < t.period);                  /* the storm sweeps faster */
    CHECK(wiper_timing(WIPER_LIGHT).period > t.period);                  /* a light rain more slowly */
    CHECK(wiper_timing(WIPER_LIGHT).sweep < wiper_timing(WIPER_LIGHT).period);   /* and with a pause */
    CHECK_EQ_INT((int)(wiper_fraction(0, t) * 1000), 0);                 /* the cycle starts parked */
    CHECK_EQ_INT((int)(wiper_fraction((uint32_t)t.sweep / 2, t) * 1000 + 0.5f), 1000);   /* the top at the sweep's half */
    float last = -1.0f;
    for (int p = 0; p <= t.sweep / 2; p++) {
        const float f = wiper_fraction((uint32_t)p, t);
        CHECK(f >= last - 1e-6f);                                          /* up: never back */
        last = f;
    }
    CHECK_EQ_INT((int)(wiper_fraction((uint32_t)t.sweep, t) * 1000), 0);   /* back at the park when the sweep is done */
    CHECK_EQ_INT((int)(wiper_fraction((uint32_t)t.period * 5, t) * 1000), 0);   /* every cycle the same */
    const WiperTiming none = wiper_timing(0);
    CHECK_EQ_INT(none.period, 0);                                          /* dry: no sweep */
    CHECK_EQ_INT((int)(wiper_fraction(77, none) * 1000), 0);
}

static void test_the_blade_goes_from_the_park_to_the_top(void)
{
    const WiperBlade b = blade();
    CHECK(fabsf(wiper_angle(&b, 0.0f) - b.park) < 1e-5f);
    CHECK(fabsf(wiper_angle(&b, 1.0f) - b.top) < 1e-5f);
    const float mid = wiper_angle(&b, 0.5f);
    CHECK(mid < b.park && mid > b.top);
}

static void test_a_point_in_the_arc_is_swept_and_outside_it_is_not(void)
{
    const WiperBlade b = blade();
    float f;
    CHECK(wiper_point_fraction(&b, 300.0f, 400.0f - 150.0f, &f));          /* straight above the pivot, inside the arc (angle 90 degrees is between 72 and 162) */
    CHECK(f > 0.0f && f < 1.0f);
    CHECK(!wiper_point_fraction(&b, 300.0f, 400.0f - 260.0f, &f));         /* beyond the blade's length */
    CHECK(!wiper_point_fraction(&b, 300.0f, 400.0f + 100.0f, &f));         /* below the pivot: outside the arc */
    CHECK(!wiper_point_fraction(&b, 300.0f + 150.0f, 400.0f, &f));         /* to the right: the park is to the left */
    CHECK(wiper_point_fraction(&b, 300.0f + 150.0f * cosf(2.6179939f), 400.0f - 150.0f * sinf(2.6179939f), &f) && f < 0.2f);   /* 150 degrees: near the park, the start of the sweep */
    CHECK(!wiper_point_fraction(&b, 300.0f - 150.0f, 400.0f, &f));         /* behind the park (180 degrees): the blade never goes there */
}

static void test_a_pass_is_remembered_from_when_the_blade_crossed_the_point(void)
{
    const WiperBlade b = blade();
    const WiperTiming t = wiper_timing(WIPER_RAIN);
    const float x = 300.0f, y = 400.0f - 150.0f;
    float f;
    CHECK(wiper_point_fraction(&b, x, y, &f));
    CHECK_EQ_INT((int)wiper_last_pass(&b, t, 0, x, y), -1);                  /* nothing passed yet in the first tick */
    const int64_t up = (int64_t)(f * (float)t.sweep / 2.0f);
    CHECK(wiper_last_pass(&b, t, (uint32_t)(up + 1), x, y) >= 0);           /* crossed on the way up */
    CHECK(wiper_last_pass(&b, t, (uint32_t)(up + 1), x, y) <= up + 1);
    const int64_t late = wiper_last_pass(&b, t, (uint32_t)t.sweep, x, y);   /* by the end of the sweep it also came back over it */
    CHECK(late > up);
    CHECK(wiper_last_pass(&b, t, (uint32_t)(t.period * 3 + 1), x, y) >= (int64_t)t.period * 2);   /* later cycles: the latest */
    CHECK_EQ_INT((int)wiper_last_pass(&b, wiper_timing(0), 500, x, y), -1);  /* dry: never */
    CHECK_EQ_INT((int)wiper_last_pass(&b, t, 100, 300.0f + 150.0f, 400.0f), -1);   /* a point outside the arc is never passed */
}

static void test_a_wiped_drop_stays_off_until_its_next_life(void)
{
    WiperPass pass = { { blade(), blade() }, 1, wiper_timing(WIPER_STORM), NULL, 0.0f, 0.0f, 1.0f, 0, NULL, { 0 } };
    int found = 0, wiped = 0;
    for (int d = 0; d < 40; d++)
        for (uint32_t tick = 0; tick < 600; tick += 3) {
            int32_t plain[4], with[4];
            const int a = weather_lens_drop(d, tick, 0, 640, 480, 320, 100, plain);
            const int b = weather_lens_drop_wiped(d, tick, 0, 640, 480, 320, 100, with, &pass);
            if (!a) CHECK_EQ_INT(b, 0);                                      /* a wiper never adds a drop */
            if (a && b) CHECK(plain[0] == with[0] && plain[1] == with[1]);   /* and does not move one */
            if (a) found++;
            if (a && !b) wiped++;
            CHECK_EQ_INT(weather_lens_drop_wiped(d, tick, 0, 640, 480, 320, 100, with, NULL), a);   /* no wipers: the drop as it was */
        }
    CHECK(found > 0);
    CHECK(wiped > 0);                                                         /* some drops are swept away */
    CHECK(wiped < found);                                                     /* and some stay: outside the arc */
}

/* the glass's plane: a blade turned up the glass is squashed on the screen (its tip traces an ellipse), the point the matrix puts it at is swept at the fraction it has, and nothing past xmax is swept */
static void test_the_blade_turns_in_the_glass_plane_and_stops_at_xmax(void)
{
    WiperBlade b = { 300.0f, 400.0f, 200.0f, 0.0f, 3.14159265f * 0.5f, 1.0f, 0.0f, 0.0f, 0.5f, 0.0f };   /* u as is, v half as long */
    float dx, dy, f;
    wiper_offset(&b, 200.0f, 0.0f, &dx, &dy);
    CHECK(dx > 199.9f && dx < 200.1f && dy > -0.1f && dy < 0.1f);          /* parked: the whole length along the edge */
    wiper_offset(&b, 200.0f, 3.14159265f * 0.5f, &dx, &dy);
    CHECK(dx > -0.1f && dx < 0.1f && dy > -100.1f && dy < -99.9f);         /* straight up the glass: half as long on the screen */
    CHECK(wiper_point_fraction(&b, 300.0f, 300.0f, &f) && f > 0.99f);      /* that point is at the top of the sweep */
    CHECK(!wiper_point_fraction(&b, 300.0f, 200.0f, &f));                  /* the circle's point is outside the squashed reach */
    b.xmax = 450.0f;
    CHECK(wiper_point_fraction(&b, 440.0f, 399.0f, &f));
    CHECK(!wiper_point_fraction(&b, 460.0f, 399.0f, &f));                  /* past the layer's right edge: not swept */
}

int main(void)
{
    test_the_sweep_is_a_triangle_and_parks();
    test_the_blade_goes_from_the_park_to_the_top();
    test_a_point_in_the_arc_is_swept_and_outside_it_is_not();
    test_a_pass_is_remembered_from_when_the_blade_crossed_the_point();
    test_a_wiped_drop_stays_off_until_its_next_life();
    test_the_blade_turns_in_the_glass_plane_and_stops_at_xmax();
    return TEST_REPORT();
}
