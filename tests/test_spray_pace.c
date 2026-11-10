/* The pace a car's spray goes by (spray_pace.h): a car that waits with a pace kept sprays none, a car that drives sprays at its own pace, a jump is not driving, a car that parks stops within
 * one window, and the same tick asked again answers the same. */
#include "game/race/spray_pace.h"
#include "tests/test.h"

enum { MS = 65536, TICK_STEP = 2 };

static int32_t drive(SprayPace *p, int32_t from, int32_t to, int32_t *x, int32_t z, int32_t metres_a_tick_16, int32_t stored)
{
    int32_t pace = 0;
    for (int32_t t = from; t <= to; t += TICK_STEP) {
        pace = spray_pace(p, t, *x, z, stored);
        *x += metres_a_tick_16 * TICK_STEP;
    }
    return pace;
}

static void a_parked_car_with_a_kept_pace_sprays_none(void)
{
    SprayPace p;
    spray_pace_clear(&p);
    int32_t x = 5 * MS;
    CHECK(drive(&p, 0, 1400, &x, 109 * MS, 0, 19 * MS) < 3 * MS);   /* 19 m/s kept, going nowhere for 23 seconds: under the sprays' thresholds */
    CHECK_EQ_INT(spray_pace(&p, 1402, x, 109 * MS, 19 * MS) < 3 * MS, 1);
}

static void a_driving_car_keeps_its_pace(void)
{
    SprayPace p;
    spray_pace_clear(&p);
    int32_t x = 0;
    const int32_t per_tick = 19 * MS / 60;   /* 19 m/s */
    const int32_t pace = drive(&p, 0, 400, &x, 0, per_tick, 19 * MS);
    CHECK_EQ_INT(pace, 19 * MS);
}

static void the_first_window_is_standing(void)
{
    SprayPace p;
    spray_pace_clear(&p);
    CHECK(spray_pace(&p, 100, 0, 0, 30 * MS) < 3 * MS);   /* nothing is known of a car just seen */
}

static void a_car_that_parks_stops_within_a_window(void)
{
    SprayPace p;
    spray_pace_clear(&p);
    int32_t x = 0;
    const int32_t per_tick = 19 * MS / 60;
    drive(&p, 0, 200, &x, 0, per_tick, 19 * MS);
    CHECK_EQ_INT(spray_pace(&p, 202, x, 0, 19 * MS) > 10 * MS, 1);   /* driving */
    int32_t pace = 0;
    int32_t parked_at = 204;
    for (int32_t t = parked_at; t < parked_at + 2 * SPRAY_PACE_WINDOW + 2; t += TICK_STEP) pace = spray_pace(&p, t, x, 0, 19 * MS);   /* it stops; the pace it kept stays */
    CHECK(pace < 3 * MS);                                              /* sprays none: gone within two windows */
    int32_t first = 0;
    for (int32_t t = parked_at + 2 * SPRAY_PACE_WINDOW + 2; t < parked_at + 3 * SPRAY_PACE_WINDOW + 2; t += TICK_STEP) first = spray_pace(&p, t, x, 0, 19 * MS);
    CHECK(first < 3 * MS);
}

static void a_jump_is_not_driving(void)
{
    SprayPace p;
    spray_pace_clear(&p);
    int32_t x = 0;
    const int32_t per_tick = 19 * MS / 60;
    drive(&p, 0, 200, &x, 0, per_tick, 19 * MS);
    x += 500 * MS;   /* recycled 500 m on */
    int32_t pace = 0;
    for (int32_t t = 202; t < 202 + SPRAY_PACE_WINDOW + 1; t += TICK_STEP) pace = spray_pace(&p, t, x, 0, 19 * MS);
    CHECK(pace < 3 * MS);                                             /* standing for the window it landed in */
    drive(&p, 220, 300, &x, 0, per_tick, 19 * MS);
    CHECK_EQ_INT(spray_pace(&p, 302, x, 0, 19 * MS), 19 * MS);        /* and it sprays at its pace again once it drives */
}

static void the_same_tick_answers_the_same(void)
{
    SprayPace p;
    spray_pace_clear(&p);
    int32_t x = 0;
    drive(&p, 0, 100, &x, 0, 19 * MS / 60, 19 * MS);
    const int32_t a = spray_pace(&p, 102, x, 0, 19 * MS), b = spray_pace(&p, 102, x + 50 * MS, 0, 5 * MS);
    CHECK_EQ_INT(a, b);
}

static void a_reverse_pace_and_a_clock_that_goes_back(void)
{
    SprayPace p;
    spray_pace_clear(&p);
    int32_t x = 0;
    const int32_t per_tick = 10 * MS / 60;
    CHECK_EQ_INT(drive(&p, 0, 300, &x, 0, per_tick, -10 * MS), 10 * MS);   /* the sign of the pace kept does not matter */
    CHECK(spray_pace(&p, 4, x, 0, 30 * MS) < 3 * MS);                       /* a new race: standing again */
}

int main(void)
{
    a_parked_car_with_a_kept_pace_sprays_none();
    a_driving_car_keeps_its_pace();
    the_first_window_is_standing();
    a_car_that_parks_stops_within_a_window();
    a_jump_is_not_driving();
    the_same_tick_answers_the_same();
    a_reverse_pace_and_a_clock_that_goes_back();
    return TEST_REPORT();
}
