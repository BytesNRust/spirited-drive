/* The body shine's pure parts (car_shine.h): the tables that lift or shade a palette entry to another entry of the same colour family, for the
 * envmap's paint and glass. */
#include "game/race/car_shine.h"
#include "game/race/car_envmap.h"
#include "game/sys/palette.h"
#include "test.h"

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

/* port (emissive lamps): an entry that glows whatever the light is no entry the shine moves: the tables map it to itself at every level */
static void test_emissive_entries_keep_their_colour(void)
{
    uint8_t pal[768], paint[CAR_SHINE_LEVELS][256], glass[CAR_SHINE_LEVELS][256], emissive[256] = { 0 };
    palette(pal);
    car_envmap_tables(pal, paint, glass);
    int moved = 0;
    for (int l = 0; l < CAR_SHINE_LEVELS; l++) moved += paint[l][RED] != RED || glass[l][RED] != RED;
    CHECK(moved > 0);   /* without the flag the red is shined, so the test means something */
    emissive[RED] = 1;
    palette_emissive_set(emissive);
    car_envmap_tables(pal, paint, glass);
    for (int l = 0; l < CAR_SHINE_LEVELS; l++) {
        CHECK_EQ_INT(paint[l][RED], RED);
        CHECK_EQ_INT(glass[l][RED], RED);
    }
    CHECK(paint[CAR_SHINE_LEVELS - 1][BLUE] != BLUE);
    palette_emissive_set(NULL);
}

/* the envmap tables (body_shine 4): the neutral level is the palette itself, a lift takes a red lighter in its own family, and the glass and
 * the paint get the same lift of their own colour (the same intensity) */
static void test_envmap_tables_lift_in_the_colours_own_family(void)
{
    uint8_t pal[768], paint[CAR_SHINE_LEVELS][256], glass[CAR_SHINE_LEVELS][256];
    palette(pal);
    car_envmap_tables(pal, paint, glass);
    CHECK_EQ_INT(paint[CAR_SHINE_NEUTRAL][RED], RED);
    CHECK_EQ_INT(glass[CAR_SHINE_NEUTRAL][BLUE], BLUE);
    CHECK(luma(pal, paint[CAR_SHINE_LEVELS - 1][RED]) > luma(pal, RED));
    CHECK(luma(pal, glass[CAR_SHINE_LEVELS - 1][BLUE]) > luma(pal, BLUE));
    CHECK(luma(pal, paint[0][RED]) < luma(pal, RED));
    CHECK_EQ_INT(paint[CAR_SHINE_LEVELS - 1][BLACK], BLACK);   /* the near-black trim stays */
}

/* a palette short of light reds has a dull pink of the red's hue (a fifth of its chroma): a lift of the red never lands on it */
static void test_a_saturated_colour_never_lifts_to_a_dull_tone_of_its_hue(void)
{
    enum { DULL_PINK = 20 };
    uint8_t pal[768], paint[CAR_SHINE_LEVELS][256], glass[CAR_SHINE_LEVELS][256];
    palette(pal);
    set(pal, DULL_PINK, 150, 108, 110);
    car_envmap_tables(pal, paint, glass);
    for (int l = 0; l < CAR_SHINE_LEVELS - 3; l++) CHECK(paint[l][RED] != DULL_PINK);   /* (the last three, the cores, may take any entry to reach near-white) */
    CHECK_EQ_INT(paint[CAR_SHINE_NEUTRAL][DULL_PINK], DULL_PINK);   /* the pink itself, a low-chroma colour, is still its own family */
}

/* a black paint's ramp is dark all through (the NSX's body entries are 4..28 a channel): the dark body tones take the lift like any paint's,
 * only the near-black trim and tyres under them stay. Otherwise a black body shows no shine, only its glass */
static void test_a_black_bodys_dark_tones_take_the_lift(void)
{
    enum { BODY_DARK = 30, BODY_DARKER = 31, TYRE = 32 };
    uint8_t pal[768], paint[CAR_SHINE_LEVELS][256], glass[CAR_SHINE_LEVELS][256];
    palette(pal);
    set(pal, BODY_DARK, 24, 28, 28), set(pal, BODY_DARKER, 12, 12, 12), set(pal, TYRE, 4, 4, 8);
    car_envmap_tables(pal, paint, glass);
    CHECK(luma(pal, paint[CAR_SHINE_LEVELS - 2][BODY_DARK]) > luma(pal, BODY_DARK) + 20);
    CHECK(luma(pal, paint[CAR_SHINE_LEVELS - 2][BODY_DARKER]) > luma(pal, BODY_DARKER) + 20);
    CHECK(luma(pal, paint[CAR_SHINE_LEVELS - 1][BODY_DARKER]) < 90);   /* a black body keeps its depth: its highlights stay dim */
    CHECK(luma(pal, paint[0][BODY_DARK]) <= luma(pal, BODY_DARK));
    for (int l = 0; l < CAR_SHINE_LEVELS; l++) CHECK_EQ_INT(paint[l][TYRE], TYRE);
}

int main(void)
{
    test_emissive_entries_keep_their_colour();
    test_envmap_tables_lift_in_the_colours_own_family();
    test_a_saturated_colour_never_lifts_to_a_dull_tone_of_its_hue();
    test_a_black_bodys_dark_tones_take_the_lift();
    return TEST_REPORT();
}
