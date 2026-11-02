/* The light made from a track's terrain (psx_light_make.h) on made-up points: the flat open case, the 16 levels, the argument checks, determinism. No game or CD data. */
#include "game/race/psx_light_make.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { NODES = 40 };

/* a flat road along z, the terrain chains going out to each side on the same plane */
static void flat(LightPos *pts)
{
    for (int n = 0; n < NODES; n++) {
        LightPos *row = pts + n * PSX_LIGHT_POINTS;
        row[0] = (LightPos){ 0, 0, (float)n * 6 };
        for (int k = 0; k < 5; k++) {
            row[1 + k] = (LightPos){ (float)(k + 1) * 8, 0, (float)n * 6 };
            row[6 + k] = (LightPos){ -(float)(k + 1) * 8, 0, (float)n * 6 };
        }
    }
}

static void test_quantised_to_sixteen_levels(void)
{
    CHECK_EQ_INT(psx_light_level(1.0f), 255);
    CHECK_EQ_INT(psx_light_level(0.0f), 0);
    CHECK_EQ_INT(psx_light_level(2.0f), 255);    /* above full: full */
    CHECK_EQ_INT(psx_light_level(-1.0f), 0);     /* below none: none */
    for (int i = 0; i <= 100; i++) CHECK(psx_light_level((float)i / 100.0f) % 17 == 0);
    CHECK(psx_light_level(0.5f) == 119 || psx_light_level(0.5f) == 136);   /* the half is one of the two levels round it */
}

static void test_a_flat_open_track_is_fully_lit(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t data[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    memset(data, 7, sizeof data);
    for (int time = PSX_SYNTH_DAWN; time <= PSX_SYNTH_DUSK; time++) {
        CHECK(psx_light_make_rows(pts, NODES, 0, time, data));
        for (size_t i = 0; i < sizeof data; i++) CHECK_EQ_INT(data[i], 255);
    }
}

static void test_the_arguments_are_checked(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t data[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    CHECK(!psx_light_make_rows(NULL, NODES, 0, PSX_SYNTH_DAY, data));
    CHECK(!psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_DAY, NULL));
    CHECK(!psx_light_make_rows(pts, 1, 0, PSX_SYNTH_DAY, data));   /* a road of one node has no direction */
    CHECK(!psx_light_make_rows(pts, NODES, 0, 99, data));          /* no such time of day */
}

static void test_the_same_points_give_the_same_bytes(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t a[NODES * PSX_LIGHT_POINTS], b[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    CHECK(psx_light_make_rows(pts, NODES, 1, PSX_SYNTH_DAY, a));
    CHECK(psx_light_make_rows(pts, NODES, 1, PSX_SYNTH_DAY, b));
    CHECK(memcmp(a, b, sizeof a) == 0);
}

int main(void)
{
    test_quantised_to_sixteen_levels();
    test_a_flat_open_track_is_fully_lit();
    test_the_arguments_are_checked();
    test_the_same_points_give_the_same_bytes();
    return TEST_REPORT();
}
