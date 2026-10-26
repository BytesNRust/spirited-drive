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

static const float UP[3] = { 0, 1, 0 };

static void test_level_roof_lighter_than_side(void)
{
    const float view[3] = { 0, -0.3f, 0.954f };   /* a camera above looking down: the exe's camera y is up */
    const float roof[3] = { 0, 1, 0 }, side[3] = { 1, 0, 0 }, low[3] = { 0, -0.2f, -0.98f };
    const float up = car_shine_level(roof, view, UP, 1), flank = car_shine_level(side, view, UP, 1), under = car_shine_level(low, view, UP, 1);
    CHECK(up > CAR_SHINE_NEUTRAL);
    CHECK(flank < CAR_SHINE_NEUTRAL);   /* a lower vertical face stays darker */
    CHECK(up > flank);
    CHECK(under <= flank + 0.01f);
}

static void rotate_x(const float v[3], float a, float out[3])
{
    out[0] = v[0], out[1] = v[1] * cosf(a) - v[2] * sinf(a), out[2] = v[1] * sinf(a) + v[2] * cosf(a);
}

/* the reflection is looked up against the world's up, not the camera's: the same panel under a camera tipped over reads as the horizon
 * says, whatever way the camera and the panel turn together */
static void test_the_level_follows_the_worlds_up_not_the_cameras(void)
{
    const float view[3] = { 0, -0.3f, 0.954f }, roof[3] = { 0, 1, 0 };
    for (int k = 0; k < 16; k++) {
        const float a = (float)k * 0.39f;
        float v[3], n[3], u[3];
        rotate_x(view, a, v), rotate_x(roof, a, n), rotate_x(UP, a, u);
        CHECK(fabsf(car_shine_level(n, v, u, 2) - car_shine_level(roof, view, UP, 2)) < 0.01f);   /* all turned together: the same */
    }
    const float grazing[3] = { 0, -0.05f, 0.9987f };   /* the reflection just over the horizon */
    float tipped[3];
    rotate_x(UP, -0.5f, tipped);
    CHECK(fabsf(car_shine_level(roof, grazing, tipped, 2) - car_shine_level(roof, grazing, UP, 2)) > 2.0f);   /* the world tips: the line moves */
}

/* a fixed horizon: a panel pitching under the sky passes from the dark lower band through the line to the bright band, and nothing else moves it */
static void test_the_horizon_is_a_line_between_a_bright_and_a_dark_band(void)
{
    const float view[3] = { 0, 0, 1 };
    float below = 0, above = 0;
    for (int st = 1; st <= 3; st++) {
        float lo = 99, hi = -99, prev = -1;
        int monotone = 1;
        for (int k = 0; k <= 40; k++) {   /* the panel's normal tilts from facing down-and-back to up-and-back */
            const float a = -0.7f + (float)k * 0.035f, n[3] = { 0, sinf(a), -cosf(a) };
            const float l = car_shine_level(n, view, UP, st);
            lo = fminf(lo, l), hi = fmaxf(hi, l);
            if (k > 0 && l < prev - 0.001f) monotone = 0;
            prev = l;
        }
        CHECK(monotone);                         /* up the panel tilts, lighter: no streaks, nothing scrolling */
        CHECK(hi - lo >= (st == 1 ? 6.0f : st == 2 ? 8.0f : 10.0f));
        if (st == 2) below = lo, above = hi;
    }
    CHECK(below < CAR_SHINE_NEUTRAL - 1.0f);     /* the lower band is dark */
    CHECK(above > CAR_SHINE_LEVELS - 5.0f);      /* the sky band is bright, within the tables' range */
    CHECK(above <= CAR_SHINE_LEVELS - 1);
}

static void test_strength_raises_the_bright_band_and_deepens_the_dark_one(void)
{
    const float view[3] = { 0, -0.3f, 0.954f }, roof[3] = { 0, 1, 0 }, low[3] = { 0, -0.2f, -0.98f };
    float sky[4], dark[4];
    for (int st = 1; st <= 3; st++) sky[st] = car_shine_level(roof, view, UP, st), dark[st] = car_shine_level(low, view, UP, st);
    CHECK(sky[2] > sky[1] + 0.5f);
    CHECK(sky[3] > sky[2] + 0.3f);
    CHECK(dark[2] < dark[1] - 0.3f);
    CHECK(dark[3] < dark[2] - 0.2f);
    for (int st = 1; st <= 3; st++) CHECK(sky[st] <= CAR_SHINE_LEVELS - 1 && dark[st] >= 0);
}

int main(void)
{
    test_tables_neutral_level_is_the_palette_itself();
    test_tables_keep_the_colour_family();
    test_tables_run_light_to_dark();
    test_tables_leave_the_markers_and_the_dark_alone();
    test_level_roof_lighter_than_side();
    test_the_level_follows_the_worlds_up_not_the_cameras();
    test_the_horizon_is_a_line_between_a_bright_and_a_dark_band();
    test_strength_raises_the_bright_band_and_deepens_the_dark_one();
    return TEST_REPORT();
}
