/* The water the cars throw up as particles of the world (rain_spray.h): dropped once a tick where the wheels pass, left there, widening and thinning, gone by their life. */
#include "game/race/rain_spray.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static int live_count(const RainSpray *s)
{
    int n = 0;
    for (int i = 0; i < RAIN_SPRAY_MAX; i++) n += s->p[i].live;
    return n;
}

static void test_dropped_once_a_tick_and_left_behind(void)
{
    static RainSpray s;
    rain_spray_clear(&s);
    const int32_t right[2] = { 0, -65536 };
    const int32_t left[3] = { 0, 0, 0 }, wheel[3] = { 0, 0, 0 };
    CHECK_EQ_INT(rain_spray_step(&s, 0, 100, left, wheel, right, 5, 20 << 16, 1), 2);
    CHECK_EQ_INT(rain_spray_step(&s, 0, 100, left, wheel, right, 5, 20 << 16, 1), 0);   /* the same tick drawn again: nothing new */
    const int32_t moved_l[3] = { 0, 0, 5 << 16 }, moved_r[3] = { 0, 0, 5 << 16 };
    CHECK_EQ_INT(rain_spray_step(&s, 0, 101, moved_l, moved_r, right, 6, 20 << 16, 1), 2);
    CHECK_EQ_INT(live_count(&s), 4);
    int32_t place[3], radius, a_place[3], a_radius;
    int density, a_density;
    const RainSprayParticle first = s.p[0];
    CHECK(rain_spray_look(&first, 101, 16, place, &radius, &density));
    CHECK(rain_spray_look(&first, 101 + RAIN_SPRAY_LIFE - 2, 16, a_place, &a_radius, &a_density));
    CHECK(a_radius > radius);                 /* it widens */
    CHECK(a_density <= density);              /* and thins */
    CHECK(abs(a_place[2] - place[2]) < (2 << 16) && abs(a_place[1] - place[1]) < (1 << 16));   /* it stays where it was dropped: the car's 5 m on does not carry it; it lifts only a little */
    CHECK(!rain_spray_look(&first, 100 + RAIN_SPRAY_LIFE, 16, place, &radius, &density));       /* gone at the end of its life */
    CHECK(!rain_spray_look(&first, 99, 16, place, &radius, &density));                          /* and not before it was dropped */
}

static void test_a_wisp_is_one_short_particle(void)
{
    static RainSpray s;
    rain_spray_clear(&s);
    const int32_t right[2] = { 0, -65536 };
    const int32_t left[3] = { -65536, 0, 0 }, wheel[3] = { 65536, 0, 0 };
    CHECK_EQ_INT(rain_spray_step_wisp(&s, 0, 100, left, wheel, right, 5, 20 << 16, 1), 1);
    CHECK_EQ_INT(rain_spray_step_wisp(&s, 0, 100, left, wheel, right, 5, 20 << 16, 1), 0);   /* the same tick again: nothing new */
    CHECK_EQ_INT(rain_spray_step_wisp(&s, 0, 101, left, wheel, right, 5, 5 << 16, 1), 0);    /* slow: nothing */
    CHECK_EQ_INT(live_count(&s), 1);
    int32_t place[3], radius;
    int density;
    CHECK(rain_spray_look(&s.p[0], 100 + RAIN_SPRAY_WISP_LIFE - 1, 16, place, &radius, &density));
    CHECK(!rain_spray_look(&s.p[0], 100 + RAIN_SPRAY_WISP_LIFE, 16, place, &radius, &density));   /* gone in a tenth of a second */
    CHECK(abs(s.p[0].x) < (1 << 16));        /* dropped between the wheels */
    rain_spray_clear(&s);
    CHECK_EQ_INT(rain_spray_step(&s, 0, 100, left, wheel, right, 5, 20 << 16, 1), 2);   /* the plume is as before */
    CHECK(rain_spray_look(&s.p[0], 100 + RAIN_SPRAY_LIFE - 1, 16, place, &radius, &density));
}

static void test_nothing_dry_or_slow(void)
{
    static RainSpray s;
    rain_spray_clear(&s);
    const int32_t right[2] = { 65536, 0 }, w[3] = { 0, 0, 0 };
    CHECK_EQ_INT(rain_spray_step(&s, 1, 10, w, w, right, 1, 20 << 16, 0), 0);   /* off the road or in the air */
    CHECK_EQ_INT(rain_spray_step(&s, 1, 11, w, w, right, 1, 5 << 16, 1), 0);    /* slow */
    CHECK_EQ_INT(live_count(&s), 0);
    CHECK_EQ_INT(rain_spray_step(&s, 99, 12, w, w, right, 1, 20 << 16, 1), 0);  /* a car that does not exist */
}

/* a rival is drawn with its matrix and its yaw stays 0: its wheels follow the matrix's heading, on the road's bends, the left and the right wheel either side of it */
static void test_a_matrix_car_has_its_wheels_where_it_heads(void)
{
    const int32_t at[3] = { 100 << 16, 0, 200 << 16 };
    int32_t forward[2], wheel[2][3], right[2];
    rain_spray_heading(1, 0, 65536, 0, forward);   /* heading east (+x), yaw 0 */
    CHECK_EQ_INT(forward[0], 65536);
    CHECK_EQ_INT(forward[1], 0);
    rain_spray_wheels(at, forward, wheel, right);
    CHECK_EQ_INT(right[0], 0);                      /* the right of a car heading +x is -z */
    CHECK_EQ_INT(right[1], -65536);
    CHECK(wheel[0][0] < at[0] && wheel[1][0] < at[0]);   /* both behind it, along the road */
    CHECK_EQ_INT(wheel[0][0], wheel[1][0]);
    CHECK(wheel[0][2] > at[2] && wheel[1][2] < at[2]);   /* the left (+z) and the right (-z) either side of it */
    CHECK_EQ_INT(wheel[0][2] - at[2], at[2] - wheel[1][2]);
    rain_spray_heading(1, 0, 0, 0, forward);        /* a degenerate matrix: the yaw's (north, +z) */
    CHECK_EQ_INT(forward[0], 0);
    CHECK_EQ_INT(forward[1], 65536);
    rain_spray_heading(0, 0x4000 << 8, 12345, 678, forward);   /* a car with no matrix: its yaw, a quarter turn */
    CHECK_EQ_INT(forward[0], 65536);
    CHECK_EQ_INT(forward[1], 0);
    rain_spray_heading(1, 0, 32886, 55230, forward);   /* an unnormalised matrix is made a unit one */
    const double len = ((double)forward[0] * forward[0] + (double)forward[1] * forward[1]) / 65536.0 / 65536.0;
    CHECK(len > 0.999 && len < 1.001);
}

static void test_a_tunnel_leaves_no_spray(void)
{
    static RainSpray s;
    rain_spray_clear(&s);
    const int32_t right[2] = { 65536, 0 }, w[3] = { 0, 0, 0 };
    rain_spray_step(&s, 0, 10, w, w, right, 1, 30 << 16, 1);
    int32_t open_place[3], mouth_place[3], shut_place[3], open_radius, mouth_radius, shut_radius;
    int open_density, mouth_density, shut_density;
    CHECK(rain_spray_look(&s.p[0], 18, 16, open_place, &open_radius, &open_density));
    CHECK(rain_spray_look(&s.p[0], 18, 8, mouth_place, &mouth_radius, &mouth_density));
    CHECK(mouth_radius < open_radius && mouth_density <= open_density);   /* at a mouth: smaller and thinner */
    CHECK(mouth_place[1] <= open_place[1]);                               /* and lower */
    CHECK(!rain_spray_look(&s.p[0], 18, 0, shut_place, &shut_radius, &shut_density));   /* port (tunnel_dry): inside a closed zone the road is dry, there is none */
}

static void test_a_clock_that_goes_back_clears_the_trail(void)
{
    static RainSpray s;
    rain_spray_clear(&s);
    const int32_t right[2] = { 65536, 0 }, w[3] = { 0, 0, 0 };
    rain_spray_step(&s, 0, 500, w, w, right, 1, 30 << 16, 1);
    CHECK_EQ_INT(live_count(&s), 2);
    rain_spray_step(&s, 0, 3, w, w, right, 1, 30 << 16, 1);   /* a restart */
    CHECK_EQ_INT(live_count(&s), 2);                          /* only the new ones */
}

static void test_wisp2(void)
{
    RainSpray s;
    rain_spray_clear(&s);
    const int32_t left[3] = { 0, 0, 0 }, wheel[3] = { 0x20000, 0, 0 }, right[2] = { 0x10000, 0 };
    CHECK_EQ_INT(rain_spray_step_wisp2(&s, 0, 100, left, wheel, right, 5, 2 << 16, 1), 2);   /* slow, still a wisp: two metres a second */
    CHECK_EQ_INT(rain_spray_step_wisp2(&s, 0, 100, left, wheel, right, 5, 2 << 16, 1), 0);   /* the same tick again */
    CHECK_EQ_INT(rain_spray_step_wisp2(&s, 0, 101, left, wheel, right, 5, 1 << 15, 1), 0);   /* under a metre a second: none */
    CHECK_EQ_INT(rain_spray_step_wisp2(&s, 0, 102, left, wheel, right, 5, 20 << 16, 0), 0);  /* off the wet road: none */
    int32_t place[3], radius, density;
    int alive = 0;
    for (int i = 0; i < RAIN_SPRAY_MAX; i++)
        if (s.p[i].live) {
            CHECK_EQ_INT(s.p[i].life, RAIN_SPRAY_WISP2_LIFE);   /* longer than the wisp's, shorter than the plume's */
            CHECK(RAIN_SPRAY_WISP2_LIFE > RAIN_SPRAY_WISP_LIFE && RAIN_SPRAY_WISP2_LIFE < RAIN_SPRAY_LIFE);
            CHECK(s.p[i].size >= 120);   /* the slow one is not a speck */
            alive += rain_spray_look(&s.p[i], 100, 16, place, &radius, &density);
        }
    CHECK_EQ_INT(alive, 2);
}

static void test_a_car_without_a_speed_goes_by_its_velocity(void)
{
    /* the traffic's physics never runs: its `speed` (0x070) stays 0 while it drives along the road at the pace of its velocity (AL1: about (-4, 16) m/s) */
    CHECK_EQ_INT(rain_spray_speed(20 << 16, 0, 0), 20 << 16);                      /* a car with a speed keeps it (the player's, a rival's) */
    CHECK_EQ_INT(rain_spray_speed(0, -4 * 65536, 16 << 16), (16 << 16) + (4 << 16) / 4); /* 00430cb0's: the larger component and a quarter of the smaller */
    CHECK_EQ_INT(rain_spray_speed(0, 16 << 16, -4 * 65536), (16 << 16) + (4 << 16) / 4); /* whichever way it points */
    CHECK_EQ_INT(rain_spray_speed(0, 0, 0), 0);                                    /* a car that stands is still */
    static RainSpray s;
    rain_spray_clear(&s);
    const int32_t right[2] = { 0, -65536 };
    const int32_t left[3] = { 0, 0, 0 }, wheel[3] = { 0, 0, 0 };
    CHECK_EQ_INT(rain_spray_step_wisp(&s, 2, 100, left, wheel, right, 5, rain_spray_speed(0, -4 * 65536, 16 << 16), 1), 1);   /* so the traffic's wisp is dropped */
    CHECK_EQ_INT(rain_spray_step_wisp2(&s, 3, 100, left, wheel, right, 5, rain_spray_speed(0, -4 * 65536, 16 << 16), 1), 2);
}

int main(void)
{
    test_dropped_once_a_tick_and_left_behind();
    test_a_wisp_is_one_short_particle();
    test_nothing_dry_or_slow();
    test_a_matrix_car_has_its_wheels_where_it_heads();
    test_a_tunnel_leaves_no_spray();
    test_a_clock_that_goes_back_clears_the_trail();
    test_wisp2();
    test_a_car_without_a_speed_goes_by_its_velocity();
    return TEST_REPORT();
}
