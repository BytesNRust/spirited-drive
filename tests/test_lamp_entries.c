/* The race rewrites some palette entries as it runs (the player's brake lamp, 0x47, and the police lights, 0xfa..0xfd): a table of the port's
 * own (fog, light, the baked light, the horizon's blend, the body shine) that picked one of them for a scenery or body colour would flash with
 * the lamp. They take the entries the exe's nearest-colour searches may pick (palette_usable, 004c5ff8) and no others. */
#include "game/race/car_shine.h"
#include "game/race/psx_lgs.h"
#include "game/race/light_tint.h"
#include "game/sys/palette.h"
#include "test.h"
#include "viewer/horizon_smooth.h"
#include "viewer/modern_road.h"

#include <string.h>

enum { LAMP = 0x47 };

static uint8_t palette[768];
static uint8_t usable_all[256], usable_race[256];
static HorizonSmooth smooth;

static void set(int entry, int r, int g, int b)
{
    palette[3 * entry] = (uint8_t)r, palette[3 * entry + 1] = (uint8_t)g, palette[3 * entry + 2] = (uint8_t)b;
}

/* the entries are reds from dark to light, 16 apart, and the lamp's own maroon (60, 12, 12) is the nearest to a good part of them */
static void setup(void)
{
    memset(palette, 0, sizeof palette);
    for (int i = 0; i < 0x40; i++) {
        const int r = 20 + 16 * (i % 14);
        set(i, r, r / 5, r / 5);
    }
    for (int i = 0x40; i < 0xfa; i++) set(i, 8 + (i * 7) % 240, 8 + (i * 13) % 240, 8 + (i * 29) % 240);
    set(LAMP, 60, 12, 12);
    set(0xfa, 60, 12, 14);
    memset(usable_all, 1, sizeof usable_all);
    usable_all[0xfe] = usable_all[0xff] = 0;
    memcpy(usable_race, usable_all, sizeof usable_race);
    usable_race[LAMP] = 0;
    for (int i = 0xfa; i < 0xfe; i++) usable_race[i] = 0;
}

/* how many of a table's entries (not the source colour itself being a lamp) are an entry the race rewrites */
static int rewritten(const uint8_t *table, int count, int stride)
{
    int n = 0;
    for (int i = 0; i < count; i++) {
        const int source = i % stride;
        if (source == LAMP || source >= 0xfa) continue;
        if (table[i] == LAMP || (table[i] >= 0xfa && table[i] < 0xfe)) n++;
    }
    return n;
}

static int count_all(void)
{
    uint8_t fog[MODERN_FOG_LEVELS][256], light[MODERN_FOG_LEVELS][256], lit[256];
    uint8_t paint[CAR_SHINE_LEVELS][256], glass[CAR_SHINE_LEVELS][256];
    static const uint8_t maroon[3] = { 60, 12, 12 };
    static const float tint[3] = { 1.0f, 1.0f, 1.0f }, half[3] = { 0.5f, 0.5f, 0.5f };
    modern_fog_tables(palette, maroon, fog);
    modern_light_tables(palette, tint, light);
    light_tint_lit_table(palette, half, lit);
    car_envmap_tables(palette, paint, glass);
    memset(&smooth, 0, sizeof smooth);
    horizon_smooth_palette(&smooth, palette);
    return rewritten(&fog[0][0], sizeof fog, 256) + rewritten(&light[0][0], sizeof light, 256) + rewritten(lit, 256, 256) +
           rewritten(&paint[0][0], sizeof paint, 256) + rewritten(&glass[0][0], sizeof glass, 256) +
           rewritten(smooth.nearest, HORIZON_SMOOTH_CELLS, HORIZON_SMOOTH_CELLS);
}

static void test_the_tables_leave_the_lamp_alone(void)
{
    setup();
    palette_usable_set(usable_all);
    const int before = count_all();   /* the exe's rule not in force: the lamp is a colour like any (so the check below can fail) */
    CHECK(before > 0);
    palette_usable_set(usable_race);
    CHECK_EQ_INT(count_all(), 0);
    palette_usable_set(usable_all);
}

static void test_a_blend_near_the_lamp_takes_the_next_colour(void)
{
    setup();
    palette_usable_set(usable_race);
    memset(&smooth, 0, sizeof smooth);
    horizon_smooth_palette(&smooth, palette);
    const uint8_t e = horizon_smooth_nearest(&smooth, 60, 12, 12);
    CHECK(e != LAMP);
    CHECK(e != 0xfa);
    palette_usable_set(usable_all);
}

/* the entries the fit gave the cars are theirs alone for the tables of the world, not for the cars' own (the body shine) */
static void test_fitted_entries_are_the_cars(void)
{
    setup();
    palette_usable_set(usable_race);
    uint8_t hidden[256] = { 0 }, fog[MODERN_FOG_LEVELS][256];
    static const uint8_t maroon[3] = { 60, 12, 12 };
    set(0x33, 61, 12, 12);   /* a fitted entry that is the nearest to the fog's colour */
    modern_fog_tables(palette, maroon, fog);
    CHECK(fog[MODERN_FOG_LEVELS - 1][0x20] == 0x33);
    hidden[0x33] = 1;
    palette_world_hide(hidden);
    CHECK_EQ_INT(palette_world_usable()[0x33], 0);
    CHECK_EQ_INT(palette_usable()[0x33], 1);
    modern_fog_tables(palette, maroon, fog);
    CHECK(fog[MODERN_FOG_LEVELS - 1][0x20] != 0x33);
    palette_world_hide(NULL);
    CHECK_EQ_INT(palette_world_usable()[0x33], 1);
    palette_usable_set(usable_all);
}

int main(void)
{
    test_the_tables_leave_the_lamp_alone();
    test_a_blend_near_the_lamp_takes_the_next_colour();
    test_fitted_entries_are_the_cars();
    return TEST_REPORT();
}
