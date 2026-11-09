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

static void test_the_streaks_are_thin_lines_and_mirror(void)
{
    uint8_t a[WATER_STREAK_BYTES], b[WATER_STREAK_BYTES];
    water_mist_streak_shape(a, 0), water_mist_streak_shape(b, 1);
    CHECK_EQ_INT(a[0], 0x7b);
    CHECK_EQ_INT(a[4] | a[5] << 8, WATER_STREAK_SIDE);
    CHECK_EQ_INT(a[6] | a[7] << 8, WATER_STREAK_SIDE);
    int lit = 0, top = 0;
    for (int y = 0; y < WATER_STREAK_SIDE; y++)
        for (int x = 0; x < WATER_STREAK_SIDE; x++) {
            const int l = a[16 + y * WATER_STREAK_SIDE + x];
            CHECK(l <= WATER_MIST_LEVELS);
            if (l > 0) lit++;
            if (l > top) top = l;
            CHECK_EQ_INT(b[16 + y * WATER_STREAK_SIDE + x], a[16 + y * WATER_STREAK_SIDE + (WATER_STREAK_SIDE - 1 - x)]);   /* the other lean is the mirror image */
        }
    CHECK_EQ_INT(top, WATER_MIST_LEVELS);                       /* the heads are as bright as the veil's top */
    CHECK(lit > 12 && lit < WATER_STREAK_SIDE * WATER_STREAK_SIDE / 8);   /* thin lines, not a patch */
    CHECK(water_mist_streak_amount() > 0 && water_mist_streak_amount() <= 255);
}

int main(void)
{
    test_the_shape_is_smooth_and_closed();
    test_it_thins_with_the_density_and_is_cooler_in_the_third();
    test_the_streaks_are_thin_lines_and_mirror();
    return TEST_REPORT();
}
