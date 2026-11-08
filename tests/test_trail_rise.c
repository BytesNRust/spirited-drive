/* The trails' rise (trail_rise.h): a puff climbs with its age to about a car's height and thins as it does. */
#include "game/race/trail_rise.h"
#include "game/race/rain_spray.h"
#include "tests/test.h"

static void test_a_puff_rises_with_age_up_to_a_cap(void)
{
    CHECK_EQ_INT(trail_rise_height(0, 20), 0);
    int32_t last = -1;
    for (int age = 0; age <= 20; age++) {
        const int32_t h = trail_rise_height(age, 20);
        CHECK(h >= last);                           /* never sinks */
        CHECK(h <= TRAIL_RISE_CAP);                 /* never past the cap */
        last = h;
    }
    CHECK(trail_rise_height(20, 20) >= TRAIL_RISE_CAP * 9 / 10);   /* it gets there by the end */
    CHECK(trail_rise_height(40, 20) <= TRAIL_RISE_CAP);            /* and a puff older than its life does not go on */
    CHECK(trail_rise_height(10, 20) > trail_rise_height(5, 20));
}

static void test_a_puff_thins_as_it_rises(void)
{
    CHECK_EQ_INT(trail_rise_fade(0, 20), 100);
    CHECK(trail_rise_fade(10, 20) < 100);
    CHECK(trail_rise_fade(20, 20) < trail_rise_fade(10, 20));
    CHECK(trail_rise_fade(20, 20) >= 20);          /* but not to nothing before its own life ends */
}

static void test_the_water_spray_rises_higher_with_the_switch(void)
{
    RainSprayParticle p = { .x = 0, .y = 1000, .z = 0, .born = 100, .size = 200, .live = 1, .life = RAIN_SPRAY_LIFE };
    int32_t place[3], radius, off_place[3];
    int density, off_density;
    CHECK(rain_spray_look(&p, 100 + RAIN_SPRAY_LIFE - 2, 16, 0, off_place, &radius, &off_density));
    CHECK(rain_spray_look(&p, 100 + RAIN_SPRAY_LIFE - 2, 16, 1, place, &radius, &density));
    CHECK(place[1] - p.y > off_place[1] - p.y);    /* higher than the little lift it had */
    CHECK(place[1] - p.y <= TRAIL_RISE_CAP);       /* but under the cap */
    CHECK(density <= off_density);                 /* and no denser */
    CHECK(rain_spray_look(&p, 100, 16, 1, place, &radius, &density));
    CHECK_EQ_INT(place[1], p.y);                   /* where it was dropped, at its birth */
    CHECK(rain_spray_look(&p, 100 + 10, 1, 1, place, &radius, &density));   /* a tunnel's mouth: hardly any */
    CHECK(place[1] - p.y <= TRAIL_RISE_CAP / 8);
}

static void test_a_wisp_does_not_rise_as_a_chimney(void)
{
    RainSprayParticle plume = { .x = 0, .y = 1000, .z = 0, .born = 100, .size = 200, .live = 1, .life = RAIN_SPRAY_LIFE };
    RainSprayParticle wisp = plume;
    wisp.life = RAIN_SPRAY_WISP2_LIFE;
    int32_t a[3], b[3], radius;
    int density;
    CHECK(rain_spray_look(&plume, 100 + RAIN_SPRAY_LIFE - 1, 16, 1, a, &radius, &density));
    CHECK(rain_spray_look(&wisp, 100 + RAIN_SPRAY_WISP2_LIFE - 1, 16, 1, b, &radius, &density));
    CHECK(b[1] - wisp.y < (a[1] - plume.y) / 2);   /* a sixth-of-a-second wisp climbs a share of what the plume's two thirds do */
}

int main(void)
{
    test_a_puff_rises_with_age_up_to_a_cap();
    test_a_puff_thins_as_it_rises();
    test_the_water_spray_rises_higher_with_the_switch();
    test_a_wisp_does_not_rise_as_a_chimney();
    return TEST_REPORT();
}
