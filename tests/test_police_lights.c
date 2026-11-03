/* The police car's flashing lights (police_lights.h): when they are on, the phase every 15 ticks, the red and blue entries, the table. */
#include "game/race/police_lights.h"
#include "test.h"

#include <string.h>

static void test_when_on(void)
{
    RaceState race;
    Car cop;
    memset(&race, 0, sizeof race);
    memset(&cop, 0, sizeof cop);
    cop.index = 2, cop.flags_51a = 4;
    CHECK(!police_lights_on(&race, &cop));                 /* no police mode */
    race.unknown_4fbab8 = 1;
    CHECK(!police_lights_on(&race, &cop));                 /* not chasing */
    cop.flags_175 = 4;
    CHECK(police_lights_on(&race, &cop));                  /* chasing */
    cop.flags_175 = 0x20;
    CHECK(!police_lights_on(&race, &cop));                 /* gave up */
    cop.flags_176[0] = 8;
    CHECK(police_lights_on(&race, &cop));                  /* the siren's flags */
    cop.flags_51a = 0;
    CHECK(!police_lights_on(&race, &cop));                 /* not active */
    cop.flags_51a = 4, cop.index = 1;
    CHECK(!police_lights_on(&race, &cop));                 /* not the police slot */
    CHECK(!police_lights_on(NULL, &cop) && !police_lights_on(&race, NULL));
}

static void test_phase(void)
{
    CHECK_EQ_INT(police_lights_phase(0), 0);
    CHECK_EQ_INT(police_lights_phase(14), 0);
    CHECK_EQ_INT(police_lights_phase(15), 1);              /* the exe's cadence: a turn every 15 ticks */
    CHECK_EQ_INT(police_lights_phase(29), 1);
    CHECK_EQ_INT(police_lights_phase(30), 0);
}

static void test_table(void)
{
    uint8_t palette[768] = { 0 }, base[256], out[256];
    for (int i = 0; i < 256; i++) base[i] = (uint8_t)(255 - i);
    palette[3 * 7] = 250, palette[3 * 7 + 1] = 10, palette[3 * 7 + 2] = 10;        /* a red at 7 */
    palette[3 * 9] = 10, palette[3 * 9 + 1] = 10, palette[3 * 9 + 2] = 250;        /* a blue at 9 */
    palette[3 * 0xfa] = 255, palette[3 * 0xfd + 2] = 255;                          /* the light entries themselves are not searched */
    CHECK_EQ_INT(police_lights_entry(palette, 1), 7);
    CHECK_EQ_INT(police_lights_entry(palette, 0), 9);
    police_lights_table(base, palette, 0, out);
    CHECK_EQ_INT(out[0xfa], 7);
    CHECK_EQ_INT(out[0xfb], 7);
    CHECK_EQ_INT(out[0xfc], 9);
    CHECK_EQ_INT(out[0xfd], 9);
    CHECK_EQ_INT(out[0x10], base[0x10]);                   /* the rest is the base table's */
    police_lights_table(NULL, palette, 15, out);
    CHECK_EQ_INT(out[0xfa], 9);                            /* the sides swap */
    CHECK_EQ_INT(out[0xfd], 7);
    CHECK_EQ_INT(out[0x10], 0x10);                         /* no base: the identity */
}

/* the entries set aside for the flash win over the palette's nearest, and forgetting them goes back to the search */
static void test_own_entries(void)
{
    uint8_t palette[768] = { 0 }, out[256];
    palette[3 * 7] = 250, palette[3 * 7 + 1] = 10, palette[3 * 7 + 2] = 10;
    palette[3 * 9] = 10, palette[3 * 9 + 1] = 10, palette[3 * 9 + 2] = 250;
    police_lights_set_entries(40, 41, 0, 0);
    police_lights_table(NULL, palette, 0, out);
    CHECK_EQ_INT(out[0xfa], 40);
    CHECK_EQ_INT(out[0xfb], 40);
    CHECK_EQ_INT(out[0xfc], 41);
    CHECK_EQ_INT(out[0xfd], 41);
    police_lights_table(NULL, palette, 15, out);
    CHECK_EQ_INT(out[0xfa], 41);                           /* the sides swap every 15 ticks */
    CHECK_EQ_INT(out[0xfd], 40);
    police_lights_set_entries(40, 41, 42, 43);             /* rims 40, 41 (0xfb, 0xfd) and centres 42, 43 (0xfa, 0xfc), as COPMUST.CFM's art has them */
    police_lights_table(NULL, palette, 0, out);
    CHECK_EQ_INT(out[0xfa], 42);
    CHECK_EQ_INT(out[0xfb], 40);
    CHECK_EQ_INT(out[0xfc], 43);
    CHECK_EQ_INT(out[0xfd], 41);
    police_lights_table(NULL, palette, 15, out);
    CHECK_EQ_INT(out[0xfa], 43);
    CHECK_EQ_INT(out[0xfb], 41);
    CHECK_EQ_INT(out[0xfc], 42);
    CHECK_EQ_INT(out[0xfd], 40);
    police_lights_set_entries(40, 0, 0, 0);                      /* one missing: the search */
    police_lights_table(NULL, palette, 0, out);
    CHECK_EQ_INT(out[0xfa], 7);
    police_lights_set_entries(0, 0, 0, 0);
    police_lights_table(NULL, palette, 0, out);
    CHECK_EQ_INT(out[0xfc], 9);
}

/* fewer than four free entries: two are enough (the centres are their rims), three give the red centre only, one or none forgets them (the search) */
static void test_found_two_and_three(void)
{
    uint8_t palette[768] = { 0 }, out[256];
    const uint8_t pair[4] = { 40, 41, 42, 43 };
    palette[3 * 7] = 250, palette[3 * 9 + 2] = 250;
    police_lights_set_found(pair, 2);
    police_lights_table(NULL, palette, 0, out);
    CHECK_EQ_INT(out[0xfa], 40);
    CHECK_EQ_INT(out[0xfb], 40);                           /* no centre: the rim */
    CHECK_EQ_INT(out[0xfc], 41);
    CHECK_EQ_INT(out[0xfd], 41);
    police_lights_set_found(pair, 3);
    police_lights_table(NULL, palette, 0, out);
    CHECK_EQ_INT(out[0xfa], 42);                           /* the red centre found */
    CHECK_EQ_INT(out[0xfb], 40);
    CHECK_EQ_INT(out[0xfc], 41);                           /* the blue centre not: its rim */
    CHECK_EQ_INT(out[0xfd], 41);
    police_lights_set_found(pair, 4);
    police_lights_table(NULL, palette, 0, out);
    CHECK_EQ_INT(out[0xfc], 43);
    CHECK_EQ_INT(out[0xfd], 41);
    police_lights_set_found(pair, 1);                      /* one is not a pair: the search */
    police_lights_table(NULL, palette, 0, out);
    CHECK_EQ_INT(out[0xfa], 7);
    CHECK_EQ_INT(out[0xfc], 9);
    police_lights_set_entries(0, 0, 0, 0);
}

int main(void)
{
    test_own_entries();
    test_found_two_and_three();
    test_when_on();
    test_phase();
    test_table();
    return TEST_REPORT();
}
