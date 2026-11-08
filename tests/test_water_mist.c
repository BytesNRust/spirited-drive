/* The water's veil (water_mist.h): a smooth, low shape with no hard edge, thinning with the particle's life. */
#include "game/race/water_mist.h"
#include "tests/test.h"

static void test_the_shape_is_smooth_and_closed(void)
{
    uint8_t s[WATER_MIST_BYTES];
    water_mist_shape(s);
    CHECK_EQ_INT(s[0], 0x7b);
    CHECK_EQ_INT(s[4] | s[5] << 8, WATER_MIST_W);
    CHECK_EQ_INT(s[6] | s[7] << 8, WATER_MIST_H);
    int top = 0;
    for (int y = 0; y < WATER_MIST_H; y++)
        for (int x = 0; x < WATER_MIST_W; x++) {
            const int l = s[16 + y * WATER_MIST_W + x];
            CHECK(l <= WATER_MIST_LEVELS);
            if (l > top) top = l;
            if (x == 0 || y == 0 || x == WATER_MIST_W - 1 || y == WATER_MIST_H - 1) CHECK_EQ_INT(l, 0);   /* nothing at the box */
            if (x > 0) CHECK(l - s[16 + y * WATER_MIST_W + x - 1] <= 2 && s[16 + y * WATER_MIST_W + x - 1] - l <= 2);   /* no jump: soft */
        }
    CHECK_EQ_INT(top, WATER_MIST_LEVELS);
}

static void test_it_thins_with_the_density_and_is_cooler_in_the_third(void)
{
    for (int v = 1; v <= WATER_MIST_VARIANTS; v++)
        for (int d = 1; d < WATER_MIST_DENSITIES; d++) CHECK(water_mist_amount(v, d) > water_mist_amount(v, d - 1));
    CHECK(water_mist_amount(1, 0) <= 10);   /* the end of its life is hardly there */
    uint8_t a[3], c[3];
    water_mist_colour(1, a), water_mist_colour(3, c);
    CHECK(c[0] < a[0] && c[2] <= 255);
    CHECK(water_mist_height_pct(3) < water_mist_height_pct(1));
    CHECK(!water_mist_streaks(1) && water_mist_streaks(2) && water_mist_streaks(3));
}

int main(void)
{
    test_the_shape_is_smooth_and_closed();
    test_it_thins_with_the_density_and_is_cooler_in_the_third();
    return TEST_REPORT();
}
