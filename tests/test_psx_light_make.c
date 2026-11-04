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

/* the road's points (0, 1, 6) never darken; the terrain's (2..5, 7..10) do */
static int road_point(int p) { return p == 0 || p == 1 || p == 6; }

static void test_quantised_to_sixteen_levels(void)
{
    CHECK_EQ_INT(psx_light_level(1.0f), 255);
    CHECK_EQ_INT(psx_light_level(0.0f), 0);
    CHECK_EQ_INT(psx_light_level(2.0f), 255);    /* above full: full */
    CHECK_EQ_INT(psx_light_level(-1.0f), 0);     /* below none: none */
    for (int i = 0; i <= 100; i++) CHECK_EQ_INT(psx_light_level((float)i / 100.0f), (int)lroundf((float)i * 2.55f));
    CHECK_EQ_INT(psx_light_level(0.5f), 128);
}

/* the road's points (0, 1, 6) are always full; the terrain's are Lambert shading (psx_light_make.c's LAMBERT): flat ground at noon is full (the CD's flat ground is level 15), at dawn and dusk
 * .80 of full (level 12, 204), the CD's flat ground, not 1 */
static void test_a_flat_open_track_is_lit_as_the_cd_lights_flat_ground(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t data[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    memset(data, 7, sizeof data);
    for (int time = PSX_SYNTH_DAWN; time <= PSX_SYNTH_DUSK; time++) {
        CHECK(psx_light_make_rows(pts, NODES, 0, time, data));
        const int expected = time == PSX_SYNTH_DAY ? 245 : 204;   /* flat ground by day is 0.96 */
        const int outer5[3] = { 173, 162, 142 }, outer10[3] = { 143, 166, 175 };   /* the outer points: their means */
        for (int n = 0; n < NODES; n++)
            for (int p = 0; p < PSX_LIGHT_POINTS; p++)
                CHECK_EQ_INT(data[n * PSX_LIGHT_POINTS + p], road_point(p) ? 255 : p == 5 ? outer5[time] : p == 10 ? outer10[time] : expected);
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

static void test_a_wall_darkens_the_terrain_under_it_not_the_road(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t open[NODES * PSX_LIGHT_POINTS], walled[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_DAY, open));
    for (int n = 0; n < NODES; n++)
        for (int k = 3; k < 5; k++) pts[n * PSX_LIGHT_POINTS + 6 + k].y = 30;   /* the far points of the left chain stand up: a cliff */
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_DAY, walled));
    for (int n = 0; n < NODES; n++) {
        for (int p = 0; p < PSX_LIGHT_POINTS; p++)
            if (road_point(p)) CHECK_EQ_INT(walled[n * PSX_LIGHT_POINTS + p], 255);
        CHECK(walled[n * PSX_LIGHT_POINTS + 8] < open[n * PSX_LIGHT_POINTS + 8]);   /* under the cliff: a tilted ground */
    }
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
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_DAY, open));
    CHECK(psx_light_make_rows(pts, NODES, 1, PSX_SYNTH_DAY, ring));
    CHECK(ring[0 * PSX_LIGHT_POINTS + 8] < open[0 * PSX_LIGHT_POINTS + 8]);   /* only the ring knows the rise is the node before the first */
    CHECK(ring[20 * PSX_LIGHT_POINTS + 2] >= 204);                            /* the far side of the circle is flat */
}

/* the CD's sun is fixed along the track's x axis: at dawn it is at -x, at dusk at +x. A slope rising toward +x faces -x: dawn's light, brighter than dusk's there */
static void test_a_slope_facing_the_dawn_is_lighter_than_at_dusk(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t dawn[NODES * PSX_LIGHT_POINTS], dusk[NODES * PSX_LIGHT_POINTS], day[NODES * PSX_LIGHT_POINTS], level[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_DAY, level));
    for (int n = 0; n < NODES; n++)
        for (int k = 0; k < 5; k++) pts[n * PSX_LIGHT_POINTS + 1 + k].y = (float)(k + 1) * 8 * 0.5f;   /* the right side rises toward +x, 27 degrees */
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_DAWN, dawn));
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_DUSK, dusk));
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_DAY, day));
    CHECK(dawn[20 * PSX_LIGHT_POINTS + 4] > dusk[20 * PSX_LIGHT_POINTS + 4]);
    CHECK(day[20 * PSX_LIGHT_POINTS + 4] < level[20 * PSX_LIGHT_POINTS + 4]);   /* a slope is darker than level ground at noon (the sun is overhead) */
    CHECK_EQ_INT(dusk[20 * PSX_LIGHT_POINTS + 0], 255);                          /* the road is lit whatever the time */
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

/* the night: moonlight on the terrain, the lamps' pools the light that does the work */
static void test_the_night_is_dark_on_the_terrain_and_lit_on_the_road(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t day[NODES * PSX_LIGHT_POINTS], night[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_DAY, day));
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_NIGHT, night));
    for (int n = 0; n < NODES; n++)
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
            if (road_point(p)) CHECK_EQ_INT(night[n * PSX_LIGHT_POINTS + p], 255);   /* the road is the pools' to darken, not the light's */
            else CHECK(night[n * PSX_LIGHT_POINTS + p] > 0 && night[n * PSX_LIGHT_POINTS + p] <= 90);   /* a dark night, not black */
        }
}

static void test_the_night_has_no_sun_to_cast_shadows(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t a[NODES * PSX_LIGHT_POINTS], b[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    const PsxLightSun east = { 0.0f, 0.3f }, west = { 3.14159265f, 0.3f };
    CHECK(psx_light_make_rows_sun(pts, NODES, 0, PSX_SYNTH_NIGHT, &east, a));
    CHECK(psx_light_make_rows_sun(pts, NODES, 0, PSX_SYNTH_NIGHT, &west, b));
    CHECK(memcmp(a, b, sizeof a) == 0);
}

static void test_a_lamp_pools_light_on_the_terrain_near_it(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t dark[NODES * PSX_LIGHT_POINTS], lit[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_NIGHT, dark));
    memcpy(lit, dark, sizeof lit);
    const uint32_t lamps[] = { 20 };
    psx_light_add_lamps(lit, NODES, 0, lamps, 1);
    CHECK(lit[20 * PSX_LIGHT_POINTS + 3] > dark[20 * PSX_LIGHT_POINTS + 3] + 50);   /* under the lamp: well lit */
    CHECK(lit[22 * PSX_LIGHT_POINTS + 3] > dark[22 * PSX_LIGHT_POINTS + 3]);        /* two nodes off: still lifted */
    CHECK(lit[22 * PSX_LIGHT_POINTS + 3] < lit[20 * PSX_LIGHT_POINTS + 3]);         /* ... less */
    CHECK_EQ_INT(lit[34 * PSX_LIGHT_POINTS + 3], dark[34 * PSX_LIGHT_POINTS + 3]);  /* far off: as it was */
    CHECK_EQ_INT(lit[20 * PSX_LIGHT_POINTS + 0], 255);                              /* the road is as it was */
}

static void test_a_lamp_beside_the_closing_node_lights_across_it_on_a_ring(void)
{
    LightPos pts[NODES * PSX_LIGHT_POINTS];
    uint8_t ring[NODES * PSX_LIGHT_POINTS], open[NODES * PSX_LIGHT_POINTS], base[NODES * PSX_LIGHT_POINTS];
    flat(pts);
    CHECK(psx_light_make_rows(pts, NODES, 0, PSX_SYNTH_NIGHT, base));
    memcpy(ring, base, sizeof ring), memcpy(open, base, sizeof open);
    const uint32_t lamps[] = { NODES - 1 };
    psx_light_add_lamps(ring, NODES, 1, lamps, 1);
    psx_light_add_lamps(open, NODES, 0, lamps, 1);
    CHECK(ring[1 * PSX_LIGHT_POINTS + 3] > base[1 * PSX_LIGHT_POINTS + 3]);
    CHECK_EQ_INT(open[1 * PSX_LIGHT_POINTS + 3], base[1 * PSX_LIGHT_POINTS + 3]);
    const uint32_t beyond[] = { 5000 };   /* a placement past the nodes wraps on a ring and is dropped on a stage */
    memcpy(open, base, sizeof open);
    psx_light_add_lamps(open, NODES, 0, beyond, 1);
    CHECK(memcmp(open, base, sizeof open) == 0);
}

/* lamp_pool_tint: the class of a node is how strong its lamp's pool is, 4 under the lamp down to 0 beyond the reach, the strongest of two lamps, ring and stage as the pools' */
static void test_lamp_classes_follow_the_pool(void)
{
    uint8_t c[NODES];
    const uint32_t lamps[] = { 20, 24 };
    psx_light_lamp_classes(c, NODES, 0, lamps, 2);
    CHECK_EQ_INT(c[20], 4);
    CHECK(c[23] >= c[21] || c[22] >= c[21]);          /* between two lamps the nearer one's class */
    CHECK(c[17] < c[20] && c[17] > 0);                /* fading off */
    CHECK_EQ_INT(c[5], 0);                            /* far off: none */
    const uint32_t edge[] = { NODES - 1 };
    psx_light_lamp_classes(c, NODES, 1, edge, 1);
    CHECK(c[1] > 0);                                  /* a ring wraps */
    psx_light_lamp_classes(c, NODES, 0, edge, 1);
    CHECK_EQ_INT(c[1], 0);                            /* a stage does not */
    PsxLight light = { 0 };
    light.nodes = NODES;
    light.pool_class = c;
    c[7] = 3;
    CHECK_EQ_INT(psx_light_tunnel_class(&light, 7), 4);   /* a pool: class + 1 */
    CHECK_EQ_INT(psx_light_tunnel_class(&light, 8), 0);
    light.pool_class = NULL;
}

/* the override file: `node_from node_to point_from point_to value` lines, # comments, applied last */
static void test_the_override_text_is_read(void)
{
    PsxLightRule rules[8];
    const char *text = "# a shaded cutting\n"
                       "100 120 2 5 40   # the right side\n"
                       "\n"
                       "  7 7 0 10 255\r\n"
                       "bad line\n"
                       "5 4 0 0 10\n"        /* from after to: dropped */
                       "1 2 3 11 10\n"       /* a point past 10: dropped */
                       "1 2 3 4 300\n"       /* a value past 255: dropped */
                       "-1 2 3 4 10\n";      /* a negative node: dropped */
    CHECK_EQ_INT(psx_light_rules_parse(text, strlen(text), rules, 8), 2);
    CHECK_EQ_INT(rules[0].node_from, 100); CHECK_EQ_INT(rules[0].node_to, 120);
    CHECK_EQ_INT(rules[0].point_from, 2); CHECK_EQ_INT(rules[0].point_to, 5); CHECK_EQ_INT(rules[0].value, 40);
    CHECK_EQ_INT(rules[1].node_from, 7); CHECK_EQ_INT(rules[1].value, 255);
    CHECK_EQ_INT(psx_light_rules_parse(text, strlen(text), rules, 1), 1);   /* the room given is not passed */
    CHECK_EQ_INT(psx_light_rules_parse("", 0, rules, 8), 0);
    CHECK_EQ_INT(psx_light_rules_parse(NULL, 0, rules, 8), 0);
}

static void test_the_rules_set_their_points_in_the_levels(void)
{
    uint8_t data[NODES * PSX_LIGHT_POINTS];
    memset(data, 255, sizeof data);
    const PsxLightRule rules[] = { { 10, 12, 2, 4, 100 }, { NODES + 5, NODES + 6, 0, 0, 0 } };
    psx_light_apply_rules(data, NODES, rules, 2);
    for (int n = 0; n < NODES; n++)
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
            const int inside = n >= 10 && n <= 12 && p >= 2 && p <= 4;
            if (inside) CHECK_EQ_INT(data[n * PSX_LIGHT_POINTS + p], psx_light_level(100.0f / 255.0f));
            else CHECK_EQ_INT(data[n * PSX_LIGHT_POINTS + p], 255);
        }
}

enum { FOREST_NODES = 100 };

static void test_a_forest_alternates_two_dark_tones_and_lets_the_open_road_be(void)
{
    uint8_t data[FOREST_NODES * PSX_LIGHT_POINTS];
    memset(data, 255, sizeof data);
    psx_light_forest(data, FOREST_NODES, 20, 60, 8);
    const int hi = data[(20 + 12) * PSX_LIGHT_POINTS + 0], lo = data[(20 + 4) * PSX_LIGHT_POINTS + 0];   /* past the ramp: the bands' own tones */
    CHECK(hi < 255 && lo < hi && lo > 0);                                                              /* dark, and darker */
    for (int p = 0; p < PSX_LIGHT_POINTS; p++) {                                                       /* the road's points too: the whole row */
        CHECK_EQ_INT(data[(20 + 12) * PSX_LIGHT_POINTS + p], hi);
        CHECK_EQ_INT(data[(20 + 4) * PSX_LIGHT_POINTS + p], lo);
        CHECK_EQ_INT(data[(20 + 20) * PSX_LIGHT_POINTS + p], lo);                                       /* the alternation goes on, two bands a period */
        CHECK_EQ_INT(data[19 * PSX_LIGHT_POINTS + p], 255);                                             /* outside the range: untouched */
        CHECK_EQ_INT(data[61 * PSX_LIGHT_POINTS + p], 255);
    }
    CHECK(data[20 * PSX_LIGHT_POINTS] > lo);   /* the entry is eased, not a step */
    psx_light_forest(NULL, FOREST_NODES, 20, 60, 8);                                                          /* the arguments are checked */
    psx_light_forest(data, FOREST_NODES, 60, 20, 8);
    psx_light_forest(data, FOREST_NODES, 20, FOREST_NODES + 40, 0);
}

static void test_a_forest_darkens_what_is_already_dark_less_than_a_proportion(void)
{
    uint8_t data[FOREST_NODES * PSX_LIGHT_POINTS];
    memset(data, 0, sizeof data);
    data[30 * PSX_LIGHT_POINTS + 3] = 100;
    psx_light_forest(data, FOREST_NODES, 20, 60, 8);
    CHECK(data[30 * PSX_LIGHT_POINTS + 3] < 100);   /* a multiple: what the made light shaded stays shaded more */
    CHECK_EQ_INT(data[30 * PSX_LIGHT_POINTS + 4], 0);
}

static void test_the_forest_is_vertigo_ridges_only(void)
{
    uint32_t from = 0, to = 0;
    CHECK(psx_light_forest_range("TR3", &from, &to));
    CHECK(from < to && to < 1024);
    CHECK(psx_light_forest_range("tr3", &from, &to));
    CHECK(!psx_light_forest_range("TR1", &from, &to));
    CHECK(!psx_light_forest_range(NULL, &from, &to));
}

static void test_the_forest_goes_over_a_loaded_light_in_its_own_node_order(void)
{
    uint32_t from = 0, to = 0;
    CHECK(psx_light_forest_range("TR3", &from, &to));
    PsxLight cd = { 0 };   /* stands in for a CD's: 1024 nodes of rows, all full */
    cd.nodes = 1024;
    cd.data = malloc((size_t)cd.nodes * PSX_LIGHT_POINTS);
    CHECK(cd.data != NULL);
    memset(cd.data, 255, (size_t)cd.nodes * PSX_LIGHT_POINTS);
    CHECK(!psx_light_forest_apply(&cd, "TR1"));
    CHECK_EQ_INT(cd.data[(from + 4) * PSX_LIGHT_POINTS], 255);   /* another track: untouched */
    CHECK(psx_light_forest_apply(&cd, "TR3"));
    CHECK(cd.data[(from + 4) * PSX_LIGHT_POINTS] < cd.data[(from + 12) * PSX_LIGHT_POINTS]);   /* the darker band, then the dark one */
    CHECK(cd.data[(from + 12) * PSX_LIGHT_POINTS] < 255);
    CHECK_EQ_INT(cd.data[(from - 1) * PSX_LIGHT_POINTS], 255);
    CHECK_EQ_INT(cd.data[(to + 1) * PSX_LIGHT_POINTS], 255);
    cd.nodes = 500;   /* a light that does not reach the forest: nothing */
    CHECK(!psx_light_forest_apply(&cd, "TR3"));
    CHECK(!psx_light_forest_apply(NULL, "TR3"));
    free(cd.data);
}

static void test_pools_go_over_a_light_the_cd_gave(void)
{
    /* a CD night as the file holds it: 8 header bytes (the node count first), every point at 40 */
    uint8_t file[8 + NODES * PSX_LIGHT_POINTS];
    memset(file, 40, sizeof file);
    memset(file, 0, 8);
    file[0] = (uint8_t)NODES;
    PsxLight cd = { 0 };
    CHECK(psx_light_parse(file, sizeof file, NODES, &cd));
    const uint32_t lamps[] = { 20 };
    CHECK_EQ_INT(psx_light_add_pools(&cd, 0, lamps, 1, 1), 1);
    CHECK(cd.data[20 * PSX_LIGHT_POINTS + 3] > 40 + 50);                 /* the ground under the lamp is lit */
    CHECK(cd.data[22 * PSX_LIGHT_POINTS + 3] > 40);                      /* and fades with the distance */
    CHECK(cd.data[22 * PSX_LIGHT_POINTS + 3] < cd.data[20 * PSX_LIGHT_POINTS + 3]);
    CHECK_EQ_INT(cd.data[34 * PSX_LIGHT_POINTS + 3], 40);                /* far off: the CD's own */
    CHECK_EQ_INT(cd.data[20 * PSX_LIGHT_POINTS + 0], 40);                /* the road's rows are left to the pool's tint */
    CHECK(cd.pool_class != NULL && cd.pool_class[20] == 4 && cd.pool_class[34] == 0);
    const uint8_t after = cd.data[20 * PSX_LIGHT_POINTS + 3];
    CHECK_EQ_INT(psx_light_add_pools(&cd, 0, lamps, 1, 1), 0);           /* once: a second call adds nothing */
    CHECK_EQ_INT(cd.data[20 * PSX_LIGHT_POINTS + 3], after);
    PsxLight none = { 0 };
    CHECK_EQ_INT(psx_light_add_pools(&none, 0, lamps, 1, 1), 0);         /* no light, no pools */
    psx_light_free(&cd);
    CHECK_EQ_INT(cd.pooled, 0);
}

int main(void)
{
    test_pools_go_over_a_light_the_cd_gave();
    test_lamp_classes_follow_the_pool();
    test_the_forest_goes_over_a_loaded_light_in_its_own_node_order();
    test_a_forest_alternates_two_dark_tones_and_lets_the_open_road_be();
    test_a_forest_darkens_what_is_already_dark_less_than_a_proportion();
    test_the_forest_is_vertigo_ridges_only();
    test_the_override_text_is_read();
    test_the_rules_set_their_points_in_the_levels();
    test_the_night_is_dark_on_the_terrain_and_lit_on_the_road();
    test_the_night_has_no_sun_to_cast_shadows();
    test_a_lamp_pools_light_on_the_terrain_near_it();
    test_a_lamp_beside_the_closing_node_lights_across_it_on_a_ring();
    test_smoothing_spreads_a_spike_and_keeps_the_road();
    test_smoothing_wraps_a_ring();
    test_a_slope_facing_the_dawn_is_lighter_than_at_dusk();
    test_each_time_has_a_sun();
    test_a_wall_darkens_the_terrain_under_it_not_the_road();
    test_a_ring_sees_across_its_closing_node();
    test_quantised_to_sixteen_levels();
    test_a_flat_open_track_is_lit_as_the_cd_lights_flat_ground();
    test_the_arguments_are_checked();
    test_the_same_points_give_the_same_bytes();
    return TEST_REPORT();
}
