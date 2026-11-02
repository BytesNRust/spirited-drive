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
    const PsxLightSun overhead = { 0.0f, 1.5f };   /* the sun overhead: the cliff takes sky, and tilts the ground it stands on */
    CHECK(psx_light_make_rows_sun(pts, NODES, 0, PSX_SYNTH_DAY, &overhead, data));
    for (int n = 0; n < NODES; n++) {
        for (int p = 0; p < PSX_LIGHT_POINTS; p++)
            if (road_point(p)) CHECK_EQ_INT(data[n * PSX_LIGHT_POINTS + p], 255);
        for (int p = 2; p <= 5; p++) CHECK(data[n * PSX_LIGHT_POINTS + p] >= 221);   /* the open side is as it was (nearly: the cliff is far) */
        CHECK(data[n * PSX_LIGHT_POINTS + 8] < (n >= 5 && n < NODES - 5 ? 200 : 215));   /* under the cliff (the road's ends see half of it) */
    }
}

static void test_a_bridge_darkens_the_terrain_below_its_deck(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t data[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    for (int n = 10; n < 30; n++) pts[n * PSX_LIGHT_POINTS].y = 6;   /* the deck over a flat valley: the centre row lifted */
    const PsxLightSun from_left = { 3.14159265f, 0.3f };   /* a low sun over the deck: its shadow falls on the ground beside it */
    CHECK(psx_light_make_rows_sun(pts, NODES, 0, PSX_SYNTH_DAY, &from_left, data));
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
    for (int n = NODES - 6; n < NODES; n++)
        for (int p = 8; p <= 10; p++) pts[n * PSX_LIGHT_POINTS + p].y = 40;   /* a rise at the end, beside node 1 round the ring */
    const PsxLightSun overhead = { 0.0f, 1.5f };
    CHECK(psx_light_make_rows_sun(pts, NODES, 0, PSX_SYNTH_DAY, &overhead, open));
    CHECK(psx_light_make_rows_sun(pts, NODES, 1, PSX_SYNTH_DAY, &overhead, ring));
    CHECK(ring[1 * PSX_LIGHT_POINTS + 8] < open[1 * PSX_LIGHT_POINTS + 8]);   /* only the ring knows the rise is next door */
    CHECK(ring[20 * PSX_LIGHT_POINTS + 2] >= 238);                            /* the far side of the circle is open */
}

/* the sun: its direction and what it does to a slope and behind a cliff */
static void test_a_slope_facing_the_sun_is_lighter_than_one_facing_away(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t toward[NODES * PSX_LIGHT_POINTS], away[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    for (int n = 0; n < NODES; n++)
        for (int k = 0; k < 5; k++) pts[n * PSX_LIGHT_POINTS + 1 + k].y = (float)(k + 1) * 8 * 0.5f;   /* the right side rises toward +x, 27 degrees */
    const PsxLightSun east = { 0.0f, 0.6f }, west = { 3.14159265f, 0.6f };   /* the sun over +x, over -x: the slope rising toward +x faces -x */
    CHECK(psx_light_make_rows_sun(pts, NODES, 0, PSX_SYNTH_DAY, &west, toward));
    CHECK(psx_light_make_rows_sun(pts, NODES, 0, PSX_SYNTH_DAY, &east, away));
    CHECK(toward[20 * PSX_LIGHT_POINTS + 4] > away[20 * PSX_LIGHT_POINTS + 4]);
    CHECK(away[20 * PSX_LIGHT_POINTS + 4] > 0);   /* the sky still lights it */
    CHECK_EQ_INT(away[20 * PSX_LIGHT_POINTS + 0], 255);   /* the road is lit whatever the sun does */
}

static void test_a_cliff_between_a_point_and_the_sun_shades_it(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t lit[NODES * PSX_LIGHT_POINTS], shaded[NODES * PSX_LIGHT_POINTS], high[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    for (int n = 0; n < NODES; n++) pts[n * PSX_LIGHT_POINTS + 6 + 3].y = 24;   /* a cliff on the left, 32 units out, 24 high */
    const PsxLightSun from_left = { 3.14159265f, 0.5f }, from_right = { 0.0f, 0.5f }, overhead = { 3.14159265f, 1.4f };
    CHECK(psx_light_make_rows_sun(pts, NODES, 0, PSX_SYNTH_DAY, &from_right, lit));
    CHECK(psx_light_make_rows_sun(pts, NODES, 0, PSX_SYNTH_DAY, &from_left, shaded));
    CHECK(psx_light_make_rows_sun(pts, NODES, 0, PSX_SYNTH_DAY, &overhead, high));
    CHECK(shaded[20 * PSX_LIGHT_POINTS + 8] < lit[20 * PSX_LIGHT_POINTS + 8]);   /* the foot of the cliff, the sun behind it */
    CHECK(high[20 * PSX_LIGHT_POINTS + 8] >= shaded[20 * PSX_LIGHT_POINTS + 8]);  /* the sun high: the shadow is short */
}

static void test_each_time_has_a_sun(void)
{
    PsxLightSun dawn, day, dusk;
    psx_light_sun_default(PSX_SYNTH_DAWN, &dawn), psx_light_sun_default(PSX_SYNTH_DAY, &day), psx_light_sun_default(PSX_SYNTH_DUSK, &dusk);
    CHECK(day.elevation > dawn.elevation && day.elevation > dusk.elevation);   /* the day's sun is the highest */
    CHECK(dawn.elevation > 0.05f && dusk.elevation > 0.05f);                   /* none is under the horizon */
}

/* smoothing: 1-2-1 along the nodes and along each chain, the road's points left out */
static void test_smoothing_spreads_a_spike_and_keeps_the_road(void)
{
    float light[NODES * PSX_LIGHT_POINTS];
    for (int i = 0; i < NODES * PSX_LIGHT_POINTS; i++) light[i] = 1.0f;
    light[20 * PSX_LIGHT_POINTS + 4] = 0.0f;   /* one dark terrain point in a lit field */
    psx_light_smooth(light, NODES, 0);
    CHECK(fabsf(light[20 * PSX_LIGHT_POINTS + 4] - 0.75f) < 1e-4f);   /* the spike is lifted (1-2-1 both ways: a quarter of the dark stays) ... */
    CHECK(light[19 * PSX_LIGHT_POINTS + 4] < 1.0f && light[21 * PSX_LIGHT_POINTS + 4] < 1.0f);   /* ... its neighbours along the road taken down */
    CHECK(light[20 * PSX_LIGHT_POINTS + 3] < 1.0f && light[20 * PSX_LIGHT_POINTS + 5] < 1.0f);   /* ... and along the chain */
    CHECK(light[20 * PSX_LIGHT_POINTS + 9] == 1.0f && light[15 * PSX_LIGHT_POINTS + 4] == 1.0f);  /* the far and the other side are as they were */
    for (int n = 0; n < NODES; n++)
        for (int p = 0; p < 7; p += (p == 0 ? 1 : p == 1 ? 5 : 1))
            if (p == 0 || p == 1 || p == 6) CHECK(light[n * PSX_LIGHT_POINTS + p] == 1.0f);   /* the road never darkens */
    float sum = 0;
    for (int i = 0; i < NODES * PSX_LIGHT_POINTS; i++) sum += light[i];
    CHECK(fabsf(sum - ((float)NODES * PSX_LIGHT_POINTS - 1.0f)) < 1e-3f);   /* the dark is spread, not lost */
}

static void test_smoothing_wraps_a_ring(void)
{
    float open[NODES * PSX_LIGHT_POINTS], ring[NODES * PSX_LIGHT_POINTS];
    for (int i = 0; i < NODES * PSX_LIGHT_POINTS; i++) open[i] = ring[i] = 1.0f;
    open[(NODES - 1) * PSX_LIGHT_POINTS + 4] = ring[(NODES - 1) * PSX_LIGHT_POINTS + 4] = 0.0f;
    psx_light_smooth(open, NODES, 0);
    psx_light_smooth(ring, NODES, 1);
    CHECK(open[0 * PSX_LIGHT_POINTS + 4] == 1.0f);   /* the end of a stage does not meet its start */
    CHECK(ring[0 * PSX_LIGHT_POINTS + 4] < 1.0f);    /* a ring does */
}

int main(void)
{
    test_smoothing_spreads_a_spike_and_keeps_the_road();
    test_smoothing_wraps_a_ring();
    test_a_slope_facing_the_sun_is_lighter_than_one_facing_away();
    test_a_cliff_between_a_point_and_the_sun_shades_it();
    test_each_time_has_a_sun();
    test_a_wall_darkens_the_terrain_under_it_not_the_road();
    test_a_bridge_darkens_the_terrain_below_its_deck();
    test_a_ring_sees_across_its_closing_node();
    test_quantised_to_sixteen_levels();
    test_a_flat_open_track_is_fully_lit();
    test_the_arguments_are_checked();
    test_the_same_points_give_the_same_bytes();
    return TEST_REPORT();
}
