/* The body shine's pure parts (car_shine.h): the level a surface takes from its normal, and the tables that brighten or darken a
 * palette entry to another entry of the same colour family. */
#include "game/race/car_shine.h"
#include "test.h"

#include <math.h>
#include <string.h>

enum { RED = 10, RED_LIGHT = 11, RED_DARK = 12, ORANGE = 13, BLUE = 14, BLUE_LIGHT = 15, GREY = 16, GREY_LIGHT = 17, GREY_DARK = 18,
       BLACK = 19 };

static void set(uint8_t *pal, int entry, int r, int g, int b)
{
    pal[3 * entry] = (uint8_t)r, pal[3 * entry + 1] = (uint8_t)g, pal[3 * entry + 2] = (uint8_t)b;
}

static void palette(uint8_t pal[768])
{
    memset(pal, 0, 768);
    set(pal, RED, 170, 20, 20), set(pal, RED_LIGHT, 225, 70, 60), set(pal, RED_DARK, 110, 10, 10);
    set(pal, ORANGE, 230, 120, 20);   /* a different hue family: 20 degrees or more from the red */
    set(pal, BLUE, 30, 50, 170), set(pal, BLUE_LIGHT, 90, 120, 230);
    set(pal, GREY, 120, 120, 124), set(pal, GREY_LIGHT, 190, 190, 194), set(pal, GREY_DARK, 70, 70, 72);
    set(pal, BLACK, 4, 4, 4);
}

static float luma(const uint8_t *pal, int e) { return 0.3f * pal[3 * e] + 0.59f * pal[3 * e + 1] + 0.11f * pal[3 * e + 2]; }

static void test_tables_neutral_level_is_the_palette_itself(void)
{
    uint8_t pal[768], t[CAR_SHINE_LEVELS][256];
    palette(pal);
    car_shine_tables(pal, t);
    for (int i = 0; i < 256; i++) CHECK_EQ_INT(t[CAR_SHINE_NEUTRAL][i], i);
}

static void test_tables_keep_the_colour_family(void)
{
    uint8_t pal[768], t[CAR_SHINE_LEVELS][256];
    palette(pal);
    car_shine_tables(pal, t);
    CHECK_EQ_INT(t[CAR_SHINE_LEVELS - 1][RED], RED_LIGHT);   /* the top level: the lighter red, not the orange or a grey */
    CHECK_EQ_INT(t[0][RED], RED_DARK);                       /* the bottom: the darker red */
    CHECK_EQ_INT(t[CAR_SHINE_LEVELS - 1][BLUE], BLUE_LIGHT);
    CHECK_EQ_INT(t[CAR_SHINE_LEVELS - 1][GREY], GREY_LIGHT); /* a grey stays a grey */
    CHECK_EQ_INT(t[0][GREY], GREY_DARK);
    for (int l = 0; l < CAR_SHINE_LEVELS; l++) {
        CHECK(t[l][RED] != ORANGE && t[l][RED] != BLUE && t[l][RED] != GREY);
        CHECK(t[l][BLUE] != RED && t[l][GREY] != RED);
    }
}

static void test_tables_run_light_to_dark(void)
{
    uint8_t pal[768], t[CAR_SHINE_LEVELS][256];
    palette(pal);
    car_shine_tables(pal, t);
    for (int l = 1; l < CAR_SHINE_LEVELS; l++) CHECK(luma(pal, t[l][RED]) >= luma(pal, t[l - 1][RED]));
    for (int l = 1; l < CAR_SHINE_LEVELS; l++) CHECK(luma(pal, t[l][GREY]) >= luma(pal, t[l - 1][GREY]));
}

static void test_tables_leave_the_markers_and_the_dark_alone(void)
{
    uint8_t pal[768], t[CAR_SHINE_LEVELS][256];
    palette(pal);
    car_shine_tables(pal, t);
    for (int l = 0; l < CAR_SHINE_LEVELS; l++) {
        CHECK_EQ_INT(t[l][0xfe], 0xfe);   /* the shadow and see-through texel markers are never chosen nor changed */
        CHECK_EQ_INT(t[l][0xff], 0xff);
    }
    for (int l = CAR_SHINE_NEUTRAL; l < CAR_SHINE_LEVELS; l++) CHECK_EQ_INT(t[l][BLACK], BLACK);   /* near black takes no light */
}

static void test_level_roof_lighter_than_side(void)
{
    const float view[3] = { 0, 0.3f, 0.954f };   /* a camera above looking down: camera y is down */
    const float roof[3] = { 0, -1, 0 }, side[3] = { 1, 0, 0 }, low[3] = { 0, 0.2f, -0.98f };
    const float up = car_shine_level(roof, view, 0), flank = car_shine_level(side, view, 0), under = car_shine_level(low, view, 0);
    CHECK(up > CAR_SHINE_NEUTRAL);
    CHECK(flank < CAR_SHINE_NEUTRAL);   /* a lower vertical face stays darker */
    CHECK(up > flank);
    CHECK(under <= flank + 0.01f);
}

static void test_level_in_range_and_moves_with_the_phase(void)
{
    const float view[3] = { 0, 0.3f, 0.954f };
    int moved = 0;
    for (int k = 0; k < 24; k++) {
        const float a = (float)k * 0.2618f, n[3] = { sinf(a), -cosf(a) * 0.8f, cosf(a) * 0.6f };
        const float len = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]), u[3] = { n[0] / len, n[1] / len, n[2] / len };
        for (int p = 0; p < 6; p++) {
            const float level = car_shine_level(u, view, (float)p * 0.37f);
            CHECK(level >= 0 && level <= CAR_SHINE_LEVELS - 1);
            if (fabsf(level - car_shine_level(u, view, 0)) > 0.5f) moved = 1;
        }
    }
    CHECK(moved);   /* the travel scroll slides the highlights: some panel changes with it */
}

int main(void)
{
    test_tables_neutral_level_is_the_palette_itself();
    test_tables_keep_the_colour_family();
    test_tables_run_light_to_dark();
    test_tables_leave_the_markers_and_the_dark_alone();
    test_level_roof_lighter_than_side();
    test_level_in_range_and_moves_with_the_phase();
    return TEST_REPORT();
}
