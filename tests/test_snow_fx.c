/* The falling snow's depth (weather_snow.h, port snow_fx): flakes that are less regular, that lean with the car's speed, a storm whose wind swings, and fewer lens marks than the rain's drops. */
#include "tests/test.h"

#include "game/race/weather.h"
#include "game/race/weather_snow.h"

#include <stdlib.h>

enum { W = 640, H = 480, SPEED_30 = 30 << 16 };

static void test_the_wind_swings_both_ways(void)
{
    CHECK_EQ_INT(weather_snow_wind(0), 0);
    CHECK(weather_snow_wind(SNOW_DRIFT_PERIOD_TICKS / 4) > 0);
    CHECK(weather_snow_wind(SNOW_DRIFT_PERIOD_TICKS * 3 / 4) < 0);
    for (uint32_t t = 0; t < 3 * SNOW_DRIFT_PERIOD_TICKS; t += 17) CHECK(abs(weather_snow_wind(t)) <= SNOW_WIND_MAX);
    CHECK_EQ_INT(weather_snow_wind(SNOW_DRIFT_PERIOD_TICKS), weather_snow_wind(0));   /* it comes round */
}

static void test_flakes_stay_in_the_box_and_repeat(void)
{
    for (int level = SNOW_LIGHT; level <= SNOW_HEAVY; level++)
        for (uint32_t tick = 0; tick < 4000; tick += 61)
            for (int i = 0; i < weather_snow_count(W, H, level); i += 7) {
                int32_t a[3], b[3];
                weather_snow_flake_fx(i, tick, level, W, H, SPEED_30, W / 2, H / 3, a);
                weather_snow_flake_fx(i, tick, level, W, H, SPEED_30, W / 2, H / 3, b);
                CHECK(a[0] == b[0] && a[1] == b[1] && a[2] == b[2]);   /* the same tick draws the same flake */
                if (a[2] == 0) continue;                              /* a flake the lean has taken out of the view */
                CHECK(a[0] >= 0 && a[0] < W && a[1] >= 0 && a[1] < H);
                CHECK(a[2] >= 1 && a[2] <= SNOW_SIZE_MAX);
            }
}

static void test_the_flakes_are_less_regular(void)
{
    int different = 0, total = 0;
    for (int i = 0; i < 200; i++) {
        int32_t base[3], fx[3];
        weather_snow_flake(i, 500, SNOW_LIGHT, W, H, base);
        weather_snow_flake_fx(i, 500, SNOW_LIGHT, W, H, 0, W / 2, H / 2, fx);
        total++;
        different += base[0] != fx[0] || base[1] != fx[1];
    }
    CHECK(different > total * 8 / 10);
}

static void test_standing_still_is_the_same_animation_with_a_little_randomness(void)
{
    long far = 0;
    for (int i = 0; i < 200; i++) {
        int32_t base[3], fx[3];
        weather_snow_flake(i, 500, SNOW_LIGHT, W, H, base);
        weather_snow_flake_fx(i, 500, SNOW_LIGHT, W, H, 0, W / 2, H / 2, fx);
        if (fx[2] == 0) continue;
        const int dx = abs(fx[0] - base[0]), dy = abs(fx[1] - base[1]);
        far += (dx > W / 20 && dx < W - W / 20) || dy > H / 10;
    }
    CHECK(far < 10);   /* a flake stays near where it was: a few percent of the box at most */
}

/* no flake wobbles from side to side: in still air a flake falls in a straight line, and in the storm it moves only with the slow wind */
static void test_no_flake_sways_side_to_side(void)
{
    for (int i = 0; i < 200; i++) {
        int32_t first[3];
        weather_snow_flake_fx(i, 0, SNOW_LIGHT, W, H, 0, W / 2, H / 2, first);
        for (uint32_t tick = 1; tick < 600; tick += 7) {
            int32_t now[3];
            weather_snow_flake_fx(i, tick, SNOW_LIGHT, W, H, 0, W / 2, H / 2, now);
            CHECK_EQ_INT(now[0], first[0]);   /* the same column: no wind in the light snow, no speed */
        }
    }
    for (int i = 0; i < 200; i++)
        for (uint32_t tick = 100; tick < 2000; tick += 13) {
            int32_t a[3], b[3];
            weather_snow_flake_fx(i, tick, SNOW_HEAVY, W, H, 0, W / 2, H / 2, a);
            weather_snow_flake_fx(i, tick + 1, SNOW_HEAVY, W, H, 0, W / 2, H / 2, b);
            int dx = abs(b[0] - a[0]);
            if (dx > W / 2) dx = W - dx;                     /* round the box's edge */
            CHECK(dx <= 4);                                   /* a tick of the slow wind: a few pixels, no flip-flop */
        }
}

static void test_the_speed_leans_the_flakes_away_from_the_vanishing_point(void)
{
    int out = 0, in = 0, n = 0;
    const int32_t vx = W / 2, vy = H / 3;
    for (int i = 0; i < 300; i++) {
        int32_t still[3], fast[3];
        weather_snow_flake_fx(i, 900, SNOW_LIGHT, W, H, 0, vx, vy, still);
        weather_snow_flake_fx(i, 900, SNOW_LIGHT, W, H, SPEED_30, vx, vy, fast);
        if (still[2] == 0 || fast[2] == 0 || still[1] < vy + H / 8 || abs(still[0] - vx) < W / 6) continue;
        n++;
        const int away = (still[0] > vx ? 1 : -1) * (fast[0] - still[0]);
        out += away > 0, in += away < 0;
        const int bound = (int)(0.31 * abs(still[0] - vx) * abs(still[1] - vy) / H) + 3;   /* the lean is a smaller angle than the rain's */
        CHECK(abs(fast[0] - still[0]) <= bound);
    }
    CHECK(n > 20);
    CHECK(out > in * 4);   /* below the vanishing point they flow outward */
}

static void test_the_storms_wind_changes_side_over_time(void)
{
    long early = 0, late = 0, light = 0;
    for (int i = 0; i < 300; i++) {
        int32_t a[3], b[3], c[3], d[3], e[3], f[3];
        const uint32_t t1 = SNOW_DRIFT_PERIOD_TICKS / 4 - 15, t2 = SNOW_DRIFT_PERIOD_TICKS * 3 / 4 - 15;
        weather_snow_flake_fx(i, t1, SNOW_HEAVY, W, H, 0, W / 2, H / 2, a), weather_snow_flake_fx(i, t1 + 30, SNOW_HEAVY, W, H, 0, W / 2, H / 2, b);
        weather_snow_flake_fx(i, t2, SNOW_HEAVY, W, H, 0, W / 2, H / 2, c), weather_snow_flake_fx(i, t2 + 30, SNOW_HEAVY, W, H, 0, W / 2, H / 2, d);
        weather_snow_flake_fx(i, t1, SNOW_LIGHT, W, H, 0, W / 2, H / 2, e), weather_snow_flake_fx(i, t1 + 30, SNOW_LIGHT, W, H, 0, W / 2, H / 2, f);
        int de = (b[0] - a[0] + W * 3 / 2) % W - W / 2, dl = (d[0] - c[0] + W * 3 / 2) % W - W / 2, dn = (f[0] - e[0] + W * 3 / 2) % W - W / 2;
        early += de, late += dl, light += dn;
    }
    CHECK(early / 300 > 15);       /* leaning one way... */
    CHECK(late / 300 < -15);       /* ...and later the other */
    CHECK(labs(light / 300) < 10); /* the light snow has no wind */
}

static void test_fewer_marks_on_the_lens_than_drops_of_rain(void)
{
    CHECK(weather_snow_lens_count(W, H) >= 1);
    CHECK(weather_snow_lens_count(W, H) < weather_lens_count(W, H));
}

/* the soft flake: a round sprite of ramp steps (0 none .. SNOW_RAMP the whitest), built in code for a size (1..SNOW_SIZE_MAX, near ones bigger) and the render's unit in pixels */
static void test_a_flake_is_round_and_soft(void)
{
    uint8_t a[SNOW_SPRITE_MAX * SNOW_SPRITE_MAX];
    for (int side = 1; side <= 4; side++)
        for (int size = 1; size <= SNOW_SIZE_MAX; size++) {
            const int dim = weather_snow_sprite(size, side, a);
            CHECK(dim >= 1 && dim <= SNOW_SPRITE_MAX);
            const int mid = dim / 2;
            int top = 0, levels[SNOW_RAMP + 1] = { 0 };
            for (int y = 0; y < dim; y++)
                for (int x = 0; x < dim; x++) {
                    const int v = a[y * dim + x];
                    CHECK(v >= 0 && v <= SNOW_RAMP);
                    levels[v]++;
                    if (v > top) top = v;
                    CHECK_EQ_INT(a[y * dim + x], a[x * dim + y]);                 /* the same turned over: round, not a square's tilt */
                }
            CHECK(top >= 1);
            CHECK_EQ_INT(a[mid * dim + mid] == top || a[(mid ? mid - 1 : 0) * dim + mid] == top, 1);   /* the brightest is at its heart */
            if (side >= 2 && size == SNOW_SIZE_MAX) {
                int steps = 0;
                for (int v = 1; v <= SNOW_RAMP; v++) steps += levels[v] > 0;
                CHECK(steps >= 3);                                               /* a ramp of its own colours, not a flat disc */
                CHECK(a[0] == 0 && a[dim - 1] == 0);                             /* the corners empty: not a square */
            }
        }
    uint8_t small[SNOW_SPRITE_MAX * SNOW_SPRITE_MAX], big[SNOW_SPRITE_MAX * SNOW_SPRITE_MAX];
    CHECK(weather_snow_sprite(SNOW_SIZE_MAX, 3, big) > weather_snow_sprite(1, 3, small));   /* near is bigger */
    CHECK_EQ_INT(weather_snow_sprite(1, 0, small) >= 1, 1);                                   /* a unit of 0 is taken as 1 */
}

int main(void)
{
    test_a_flake_is_round_and_soft();
    test_the_wind_swings_both_ways();
    test_flakes_stay_in_the_box_and_repeat();
    test_the_flakes_are_less_regular();
    test_standing_still_is_the_same_animation_with_a_little_randomness();
    test_no_flake_sways_side_to_side();
    test_the_speed_leans_the_flakes_away_from_the_vanishing_point();
    test_the_storms_wind_changes_side_over_time();
    test_fewer_marks_on_the_lens_than_drops_of_rain();
    return TEST_REPORT();
}
