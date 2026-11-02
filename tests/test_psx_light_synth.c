/* The light made from a road's tunnels (psx_light_synth.h) on a made-up road: which runs are tunnels, the ramp either side, the rows' values as the light
 * reads them, the ring joining a run across node 0. No game or CD data. */
#include "game/race/psx_light_synth.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { NODES = 64 };

static void road(uint8_t *property, int from, int to)   /* nodes from..to have the tunnel bit */
{
    for (int i = from; i <= to; i++) property[i] |= PSX_SYNTH_PROPERTY_TUNNEL;
}

static void test_runs_and_short_runs(void)
{
    uint8_t property[NODES] = { 0 }, flags[NODES];
    road(property, 10, 10 + PSX_SYNTH_MIN_RUN - 1);        /* just long enough: a tunnel */
    road(property, 40, 40 + PSX_SYNTH_MIN_RUN - 2);        /* one short: an overpass */
    road(property, 46, 58);                                /* 13 nodes, long enough ... */
    for (int i = 46; i <= 58; i++) property[i] |= PSX_SYNTH_PROPERTY_OPEN;                /* the tunnel bit with the open one: a bridge, not a tunnel */
    property[60] = 3;                                      /* the ordinary property: no tunnel bit */
    psx_light_synth_tunnels(property, NODES, 0, flags);
    for (int i = 0; i < NODES; i++) {
        const int tunnel = i >= 10 && i < 10 + PSX_SYNTH_MIN_RUN;
        CHECK_EQ_INT(flags[i], tunnel);
    }
}

/* TR7's steelworks hall: property 17 (the hall bit with the open one) along the start straight, across node 0 of the ring, is a hall (flag 2, not a tunnel's 1); a short one is nothing */
static void test_hall_is_a_tunnel(void)
{
    uint8_t property[NODES], flags[NODES];
    for (int i = 0; i < NODES; i++) property[i] = 3;
    for (int i = NODES - 5; i < NODES; i++) property[i] = 17;
    for (int i = 0; i < 10; i++) property[i] = 17;              /* 15 nodes across the line */
    property[30] = property[31] = 17;                           /* two nodes of the hall bit: too short */
    psx_light_synth_tunnels(property, NODES, 1, flags);
    CHECK_EQ_INT(flags[NODES - 5], PSX_SYNTH_FLAG_HALL);
    CHECK_EQ_INT(flags[0], PSX_SYNTH_FLAG_HALL);
    CHECK_EQ_INT(flags[9], PSX_SYNTH_FLAG_HALL);
    CHECK_EQ_INT(flags[10], 0);
    CHECK_EQ_INT(flags[30], 0);
}

/* a hall's rows leave the world's light (254 of 255 at the one point) and mark the cars' alternation: warm under a lamp (even node), dim between, as a tunnel's */
static void test_hall_rows_and_cars(void)
{
    uint8_t flags[NODES] = { 0 }, data[NODES * PSX_LIGHT_POINTS];
    for (int i = 10; i < 30; i++) flags[i] = PSX_SYNTH_FLAG_HALL;
    psx_light_synth_rows(flags, NODES, 0, PSX_SYNTH_DAY, data);
    for (int p = 0; p < PSX_LIGHT_POINTS; p++) CHECK(data[20 * PSX_LIGHT_POINTS + p] >= PSX_SYNTH_HALL_MARK);
    CHECK_EQ_INT(data[9 * PSX_LIGHT_POINTS + 4], PSX_LIGHT_FULL);   /* next to the hall: nothing, no doors */
    PsxLight light = { 0 };
    light.data = data, light.nodes = NODES;
    float lit[3], dim[3], open[3];
    psx_light_synth_car(&light, 20, lit);
    psx_light_synth_car(&light, 21, dim);
    psx_light_synth_car(&light, 5, open);
    CHECK(lit[0] < 1.0f && dim[0] < lit[0]);
    CHECK(open[0] == 1.0f);
}

static void test_ring_joins_a_run_across_node_zero(void)
{
    uint8_t property[NODES] = { 0 }, flags[NODES];
    road(property, 0, 5);                                  /* 6 + 8 = 14 nodes across the start line */
    road(property, NODES - 8, NODES - 1);
    psx_light_synth_tunnels(property, NODES, 1, flags);
    CHECK_EQ_INT(flags[0], 1);
    CHECK_EQ_INT(flags[5], 1);
    CHECK_EQ_INT(flags[NODES - 8], 1);
    CHECK_EQ_INT(flags[NODES - 1], 1);
    CHECK_EQ_INT(flags[6], 0);
    psx_light_synth_tunnels(property, NODES, 0, flags);    /* a stage's ends are ends: 6 and 8 are both short */
    CHECK_EQ_INT(flags[0], 0);
    CHECK_EQ_INT(flags[NODES - 1], 0);
    memset(property, PSX_SYNTH_PROPERTY_TUNNEL, sizeof property);   /* a ring that is all tunnel */
    psx_light_synth_tunnels(property, NODES, 1, flags);
    for (int i = 0; i < NODES; i++) CHECK_EQ_INT(flags[i], 1);
}

static void rows(PsxLight *light, uint8_t *data, int time)   /* a tunnel at nodes 20..39 of NODES, room for the doors either side */
{
    uint8_t property[NODES] = { 0 }, flags[NODES];
    road(property, 20, 39);
    psx_light_synth_tunnels(property, NODES, 0, flags);
    psx_light_synth_rows(flags, NODES, 0, time, data);
    light->data = data, light->nodes = NODES, light->scripted = 0, light->reversed = 0;
}

static void test_rows_open_and_tunnel(void)
{
    uint8_t data[NODES * PSX_LIGHT_POINTS];
    PsxLight light = { 0 };
    rows(&light, data, PSX_SYNTH_DAY);
    for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
        CHECK_EQ_INT(psx_light_byte(&light, 5, p), PSX_LIGHT_FULL);                                 /* far from it: the open road */
        CHECK_EQ_INT(psx_light_byte(&light, 60, p), PSX_LIGHT_FULL);
    }
    CHECK(psx_light_tunnel(&light, 20) && psx_light_tunnel(&light, 39));                            /* the rows the renderer takes as a tunnel's */
    CHECK(!psx_light_tunnel(&light, 10) && !psx_light_tunnel(&light, 17) && !psx_light_tunnel(&light, 45));
    CHECK_EQ_INT(psx_light_byte(&light, 30, 0), 255);                                               /* the CD's CY1 B inside: road 255, foot 130, beyond 0 */
    CHECK_EQ_INT(psx_light_byte(&light, 30, 1), 255);
    CHECK_EQ_INT(psx_light_byte(&light, 30, 6), 255);
    CHECK_EQ_INT(psx_light_byte(&light, 30, 2), 130);
    CHECK_EQ_INT(psx_light_byte(&light, 30, 7), 130);
    for (int p = 3; p <= 5; p++) CHECK_EQ_INT(psx_light_byte(&light, 30, p), 0);
    for (int p = 8; p <= 10; p++) CHECK_EQ_INT(psx_light_byte(&light, 30, p), 0);
    CHECK_EQ_INT(psx_light_at(&light, 30, 0), PSX_LIGHT_TUNNEL_ROAD);                               /* as the light reads it: dim road, half-light foot and wall */
    CHECK_EQ_INT(psx_light_at(&light, 30, 2), PSX_LIGHT_WALL);
    CHECK_EQ_INT(psx_light_at(&light, 30, 4), PSX_LIGHT_WALL);
}

static void test_doors_follow_the_measured_ramps(void)
{
    uint8_t data[NODES * PSX_LIGHT_POINTS];
    PsxLight light = { 0 };
    rows(&light, data, PSX_SYNTH_DAY);
    /* the points beyond: CY1 B's entry 11 @3, 99 @4 before the first node (and 0 from 2), the exit 12, 30, 125, 162, 190, 206 from the 3rd after the last */
    static const int ENTRY[] = { 0, 0, 0, 11, 99, 150, 192, 225, 255 }, EXIT[] = { 0, 0, 0, 12, 30, 125, 162, 190, 206, 230, 255 };
    for (int d = 1; d <= 8; d++) CHECK_EQ_INT(psx_light_byte(&light, (uint32_t)(20 - d), 4), ENTRY[d]);
    for (int d = 1; d <= 10; d++) CHECK_EQ_INT(psx_light_byte(&light, (uint32_t)(39 + d), 9), EXIT[d]);
    /* the foot and the road: an S over PSX_SYNTH_FOOT_RAMP nodes either side, 255 at its far end, the tunnel's values at the door; slow at both ends */
    int last = 130, first_step = 0, middle_step = 0;
    for (int d = 1; d <= PSX_SYNTH_FOOT_RAMP; d++) {
        const int v = psx_light_byte(&light, (uint32_t)(20 - d), 2);
        CHECK(v >= last);
        if (d == 1) first_step = v - 130;
        if (d == PSX_SYNTH_FOOT_RAMP / 2) middle_step = v - last;
        last = v;
    }
    CHECK_EQ_INT(last, PSX_LIGHT_FULL);
    CHECK(first_step < middle_step);
    CHECK_EQ_INT(psx_light_byte(&light, 20 - PSX_SYNTH_FOOT_RAMP, 2), PSX_LIGHT_FULL);
    CHECK_EQ_INT(psx_light_byte(&light, 19, 2), psx_light_byte(&light, 40, 2));                      /* the two doors alike */
    CHECK_EQ_INT(psx_light_byte(&light, 15, 0), psx_light_byte(&light, 15, 6));                      /* both sides of the road alike */
}

static void test_times_of_day(void)
{
    uint8_t data[NODES * PSX_LIGHT_POINTS];
    PsxLight light = { 0 };
    rows(&light, data, PSX_SYNTH_DAWN);                                                              /* CY1 A: road about 215, foot about 196 */
    CHECK_EQ_INT(psx_light_byte(&light, 30, 0), 215);
    CHECK_EQ_INT(psx_light_byte(&light, 30, 2), 196);
    CHECK_EQ_INT(psx_light_byte(&light, 30, 4), 0);
    rows(&light, data, PSX_SYNTH_DUSK);                                                              /* C: road about 214, the foot the mean of 14 and 198 */
    CHECK_EQ_INT(psx_light_byte(&light, 30, 1), 214);
    CHECK_EQ_INT(psx_light_byte(&light, 30, 2), 106);
    CHECK_EQ_INT(psx_light_byte(&light, 30, 7), 106);
    CHECK_EQ_INT(psx_light_byte(&light, 30, 9), 0);
    rows(&light, data, 7);                                                                           /* a time that is none: the day's */
    CHECK_EQ_INT(psx_light_byte(&light, 30, 2), 130);
}

static float car_green(const PsxLight *light, uint32_t node)
{
    float f[3];
    psx_light_synth_car(light, node, f);
    return f[1];
}

static void test_car_factor_follows_the_beyond_points(void)
{
    uint8_t data[NODES * PSX_LIGHT_POINTS];
    PsxLight light = { 0 };
    rows(&light, data, PSX_SYNTH_DAY);
    float f[3];
    psx_light_synth_car(&light, 5, f);                                                               /* the open road: full light */
    CHECK(f[0] == 1.0f && f[1] == 1.0f && f[2] == 1.0f);
    const float inside = car_green(&light, 30);                                                      /* an even node in the tunnel: under a lamp */
    CHECK(inside < 1.0f);
    for (int d = PSX_SYNTH_BEYOND_ENTRY; d >= 1; d--) {                                              /* going in (even nodes): never lighter, ends at the tunnel's */
        const float g = car_green(&light, (uint32_t)(20 - d - ((20 - d) & 1)));
        CHECK(g <= 1.0f && g >= inside);
    }
    CHECK(car_green(&light, 42) > car_green(&light, 30));                                            /* the exit gives it back, slowly */
    CHECK(car_green(&light, 48) < 1.0f);
    CHECK(car_green(&light, 49) == 1.0f);
    float none[3];
    psx_light_synth_car(NULL, 3, none);
    CHECK(none[0] == 1.0f && none[1] == 1.0f && none[2] == 1.0f);
}

/* the lamps: inside a tunnel the light alternates node by node, warm under a lamp (even) and a dimmer grey between (odd), the CD's RAMP context */
static void test_lamps_flicker_node_by_node(void)
{
    uint8_t data[NODES * PSX_LIGHT_POINTS];
    PsxLight light = { 0 };
    rows(&light, data, PSX_SYNTH_DAY);
    for (uint32_t node = 24; node < 36; node += 2) {
        float lit[3], dim[3];
        psx_light_synth_car(&light, node, lit);
        psx_light_synth_car(&light, node + 1, dim);
        CHECK(lit[0] > dim[0] && lit[1] > dim[1] && lit[2] > dim[2]);                                /* every channel brighter under the lamp */
        CHECK(lit[0] > lit[1] && lit[1] > lit[2]);                                                   /* yellowish: red over green over blue */
        CHECK(dim[0] == dim[1] && dim[1] == dim[2]);                                                 /* between lamps: grey */
        CHECK(lit[0] == 96.0f / 160 && dim[0] == 62.0f / 160);                                       /* the CD's values through the exe's table */
    }
    float a[3], b[3];                                                                                /* the open road does not flicker */
    psx_light_synth_car(&light, 2, a);
    psx_light_synth_car(&light, 3, b);
    CHECK(a[0] == 1.0f && b[0] == 1.0f);
    CHECK(car_green(&light, 14) < 1.0f && car_green(&light, 14) >= car_green(&light, 30));          /* a door fades the alternation in, not a step */
    CHECK(car_green(&light, 14) > car_green(&light, 16));
}

static void test_tracks(void)
{
    CHECK(psx_light_synth_track("TR6") && psx_light_synth_track("tr7"));
    CHECK(!psx_light_synth_track("TR1") && !psx_light_synth_track("CY1") && !psx_light_synth_track(NULL));
}

int main(void)
{
    test_runs_and_short_runs();
    test_hall_is_a_tunnel();
    test_hall_rows_and_cars();
    test_ring_joins_a_run_across_node_zero();
    test_rows_open_and_tunnel();
    test_doors_follow_the_measured_ramps();
    test_times_of_day();
    test_car_factor_follows_the_beyond_points();
    test_lamps_flicker_node_by_node();
    test_tracks();
    return TEST_REPORT();
}
