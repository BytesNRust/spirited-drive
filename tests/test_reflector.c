/* The reflectors' pure part (reflector.h): the shine at a point, the mask on smaller variants, the colours and the re-indexing. */
#include "game/race/reflector.h"
#include "tests/test.h"

#include <string.h>

static void test_the_shine_is_the_players_cone_and_only_in_the_dark(void)
{
    CHECK_EQ_INT(reflector_gain(50, 0, 1, 0), 0);          /* daylight: nothing */
    CHECK_EQ_INT(reflector_gain(-5, 0, 1, 1), 0);          /* behind the lamps */
    CHECK_EQ_INT(reflector_gain(0, 0, 1, 1), 0);
    CHECK_EQ_INT(reflector_gain(REFLECTOR_REACH_M, 0, 0.65, 1), 0);   /* at the reach */
    CHECK_EQ_INT(reflector_gain(REFLECTOR_REACH_M + 50, 0, 0.65, 1), 0);
    CHECK(reflector_gain(REFLECTOR_NEAR_M, 0, 0.65, 1) == 256);        /* near, on the axis: full */
}

static void test_the_shine_falls_with_distance_and_away_from_the_axis(void)
{
    int last = 257;
    for (int d = REFLECTOR_NEAR_M; d < REFLECTOR_REACH_M; d += 10) {
        const int g = reflector_gain(d, 0, 0.65, 1);
        CHECK(g <= last);
        last = g;
    }
    CHECK(reflector_gain(100, 0, 0.65, 1) > 0 && reflector_gain(190, 0, 0.65, 1) > 0);   /* still there at 100 and 190 m */
    CHECK(reflector_gain(100, 0, 0.65, 1) > reflector_gain(100, 8, 0.65, 1));            /* the centre is brighter */
    CHECK(reflector_gain(100, 8, 0.65, 1) > reflector_gain(100, 16, 0.65, 1));
    CHECK_EQ_INT(reflector_gain(100, 40, 0.65, 1), 0);                                   /* out of the cone */
    CHECK_EQ_INT(reflector_gain(100, 8, 0.65, 1), reflector_gain(100, -8, 0.65, 1));     /* symmetric */
}

static void test_the_shine_thins_above_the_lamps(void)
{
    CHECK(reflector_gain(60, 0, 3.0, 1) > 0);                      /* a sign over the road */
    CHECK_EQ_INT(reflector_gain(60, 0, 40.0, 1), 0);               /* high above: out of the beam */
    CHECK(reflector_gain(60, 0, 0.0, 1) == reflector_gain(60, 0, 0.5, 1));   /* below the lamps: the ground */
}

static void test_the_levels_cover_the_gains(void)
{
    CHECK_EQ_INT(reflector_level(0), 0);
    CHECK_EQ_INT(reflector_level(1), 1);
    CHECK_EQ_INT(reflector_level(256), REFLECTOR_LEVELS - 1);
    int last = 0;
    for (int g = 0; g <= 256; g++) {
        const int l = reflector_level(g);
        CHECK(l >= last && l < REFLECTOR_LEVELS);
        last = l;
    }
}

static void test_a_stud_survives_the_smaller_variants(void)
{
    uint8_t big[16 * 16] = { 0 }, half[8 * 8], quarter[4 * 4];
    big[5 * 16 + 9] = 1;   /* a one texel stud */
    CHECK_EQ_INT((int)reflector_mask_scale(big, 16, 16, half, 8, 8), 1);
    CHECK_EQ_INT(half[2 * 8 + 4], 1);
    CHECK_EQ_INT((int)reflector_mask_scale(half, 8, 8, quarter, 4, 4), 1);
    CHECK_EQ_INT(quarter[1 * 4 + 2], 1);
    CHECK_EQ_INT((int)reflector_mask_scale(big, 16, 16, half, 0, 8), 0);
}

static void test_the_masked_colours_are_listed_and_repainted(void)
{
    uint8_t px[8] = { 10, 10, 10, 20, 30, 20, 40, 10 }, mask[8] = { 1, 1, 0, 1, 0, 1, 0, 0 };
    uint8_t out[4];
    CHECK_EQ_INT(reflector_colours(px, mask, 8, out, 4), 2);
    CHECK_EQ_INT(out[0], 10);   /* two texels */
    CHECK_EQ_INT(out[1], 20);   /* two texels, after */
    CHECK_EQ_INT(reflector_colours(px, mask, 8, out, 1), 1);
    uint8_t entry[256];
    for (int i = 0; i < 256; i++) entry[i] = (uint8_t)i;
    entry[10] = 200, entry[20] = 201;
    CHECK_EQ_INT((int)reflector_paint(px, mask, 8, entry), 4);
    const uint8_t want[8] = { 200, 200, 10, 201, 30, 201, 40, 10 };   /* the unmasked 10s stay */
    CHECK(memcmp(px, want, 8) == 0);
    CHECK_EQ_INT((int)reflector_paint(px, mask, 8, entry), 0);        /* again: nothing */
}

static void test_a_texel_takes_the_family_of_its_colour(void)
{
    const uint8_t white[3] = { 230, 230, 225 }, grey[3] = { 90, 90, 95 }, red[3] = { 200, 30, 30 }, orange[3] = { 220, 120, 20 }, yellow[3] = { 230, 200, 20 }, green[3] = { 20, 140, 60 },
                  black[3] = { 0, 0, 0 };
    CHECK_EQ_INT(reflector_family(white), REFLECTOR_WHITE);
    CHECK_EQ_INT(reflector_family(grey), REFLECTOR_WHITE);
    CHECK_EQ_INT(reflector_family(black), REFLECTOR_WHITE);
    CHECK_EQ_INT(reflector_family(red), REFLECTOR_RED);
    CHECK_EQ_INT(reflector_family(orange), REFLECTOR_WARM);
    CHECK_EQ_INT(reflector_family(yellow), REFLECTOR_WARM);
    CHECK_EQ_INT(reflector_family(green), REFLECTOR_WARM);
}

static void test_the_level_picks_a_step_of_the_ramp(void)
{
    CHECK_EQ_INT(reflector_step(1, 1), 0);
    CHECK_EQ_INT(reflector_step(REFLECTOR_LEVELS - 1, 1), 0);
    for (int steps = 2; steps <= 4; steps++) {
        CHECK_EQ_INT(reflector_step(1, steps), 0);                               /* the dimmest first */
        CHECK_EQ_INT(reflector_step(REFLECTOR_LEVELS - 1, steps), steps - 1);    /* the brightest at the top */
        int last = 0;
        for (int l = 1; l < REFLECTOR_LEVELS; l++) {
            const int s = reflector_step(l, steps);
            CHECK(s >= last && s < steps);
            last = s;
        }
    }
}

int main(void)
{
    test_the_shine_is_the_players_cone_and_only_in_the_dark();
    test_the_shine_falls_with_distance_and_away_from_the_axis();
    test_the_shine_thins_above_the_lamps();
    test_the_levels_cover_the_gains();
    test_a_texel_takes_the_family_of_its_colour();
    test_the_level_picks_a_step_of_the_ramp();
    test_a_stud_survives_the_smaller_variants();
    test_the_masked_colours_are_listed_and_repainted();
    return TEST_REPORT();
}
