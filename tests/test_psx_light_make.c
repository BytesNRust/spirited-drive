/* The light made from a track's terrain (psx_light_make.h) on made-up points: the flat open case, the 16 levels, the argument checks, determinism. No game or CD data. */
#include "game/race/psx_light_make.h"
#include "test.h"

#include <math.h>
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

/* the road's points (0, 1, 6) never darken; the terrain's (2..5, 7..10) do */
static int road_point(int p) { return p == 0 || p == 1 || p == 6; }

static void test_a_wall_darkens_the_terrain_under_it_not_the_road(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t data[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    for (int n = 0; n < NODES; n++)
        for (int k = 3; k < 5; k++) pts[n * PSX_LIGHT_POINTS + 6 + k].y = 30;   /* the far points of the left chain stand up: a cliff */
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_DAY, data));
    for (int n = 0; n < NODES; n++) {
        for (int p = 0; p < PSX_LIGHT_POINTS; p++)
            if (road_point(p)) CHECK_EQ_INT(data[n * PSX_LIGHT_POINTS + p], 255);
        for (int p = 2; p <= 5; p++) CHECK(data[n * PSX_LIGHT_POINTS + p] >= 238);   /* the open side is as it was (nearly: the cliff is far) */
        CHECK(data[n * PSX_LIGHT_POINTS + 8] < (n >= 5 && n < NODES - 5 ? 200 : 210));   /* under the cliff (the road's ends see half of it) */
    }
}

static void test_a_bridge_darkens_the_terrain_below_its_deck(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t data[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    for (int n = 10; n < 30; n++) pts[n * PSX_LIGHT_POINTS].y = 6;   /* the deck over a flat valley: the centre row lifted */
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_DAY, data));
    CHECK_EQ_INT(data[20 * PSX_LIGHT_POINTS + 0], 255);               /* the deck is lit */
    CHECK(data[20 * PSX_LIGHT_POINTS + 2] < data[2 * PSX_LIGHT_POINTS + 2]);   /* the ground beside it, under it, is darker than the open ground */
    CHECK_EQ_INT(data[2 * PSX_LIGHT_POINTS + 2], 255);
}

/* a circle of nodes: the track closes on itself, so the nodes either side of the closing node are neighbours in space */
static void circle(LightPos *pts)
{
    const float radius = 100.0f;
    for (int n = 0; n < NODES; n++) {
        const float a = 6.2831853f * (float)n / (float)NODES, c = cosf(a), s = sinf(a);
        LightPos *row = pts + n * PSX_LIGHT_POINTS;
        row[0] = (LightPos){ radius * c, 0, radius * s };
        for (int k = 0; k < 5; k++) {
            row[1 + k] = (LightPos){ (radius + (float)(k + 1) * 8) * c, 0, (radius + (float)(k + 1) * 8) * s };
            row[6 + k] = (LightPos){ (radius - (float)(k + 1) * 8) * c, 0, (radius - (float)(k + 1) * 8) * s };
        }
    }
}

static void test_a_ring_sees_across_its_closing_node(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t open[NODES * PSX_LIGHT_POINTS], ring[NODES * PSX_LIGHT_POINTS];
    circle(pts);
    for (int n = NODES - 4; n < NODES; n++) pts[n * PSX_LIGHT_POINTS + 9].y = 40;   /* a rise at the end, beside node 1 round the ring */
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_DAY, open));
    CHECK(psx_light_make_rows(pts, NODES, 1, PSX_SYNTH_DAY, ring));
    CHECK(ring[1 * PSX_LIGHT_POINTS + 8] < open[1 * PSX_LIGHT_POINTS + 8]);   /* only the ring knows the rise is next door */
    CHECK_EQ_INT(ring[20 * PSX_LIGHT_POINTS + 2], 255);                       /* the far side of the circle is open */
}

int main(void)
{
    test_a_wall_darkens_the_terrain_under_it_not_the_road();
    test_a_bridge_darkens_the_terrain_below_its_deck();
    test_a_ring_sees_across_its_closing_node();
    test_quantised_to_sixteen_levels();
    test_a_flat_open_track_is_fully_lit();
    test_the_arguments_are_checked();
    test_the_same_points_give_the_same_bytes();
    return TEST_REPORT();
}
