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
    property[60] = 3;                                      /* the ordinary property: no tunnel bit */
    psx_light_synth_tunnels(property, NODES, 0, flags);
    for (int i = 0; i < NODES; i++) {
        const int tunnel = i >= 10 && i < 10 + PSX_SYNTH_MIN_RUN;
        CHECK_EQ_INT(flags[i], tunnel);
    }
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

static void test_rows_open_ramp_and_tunnel(void)
{
    uint8_t property[NODES] = { 0 }, flags[NODES], data[NODES * PSX_LIGHT_POINTS];
    road(property, 20, 39);
    psx_light_synth_tunnels(property, NODES, 0, flags);
    psx_light_synth_rows(flags, NODES, 0, data);
    PsxLight light = { .data = data, .nodes = NODES };
    for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
        CHECK_EQ_INT(psx_light_at(&light, 5, p), PSX_LIGHT_FULL);                                   /* far from it: the open road */
        CHECK_EQ_INT(psx_light_at(&light, 20 - PSX_SYNTH_RAMP - 1, p), PSX_LIGHT_FULL);
    }
    CHECK(!psx_light_tunnel(&light, 20 - 1));                                                       /* the ramp is not a tunnel row ... */
    CHECK(psx_light_tunnel(&light, 20) && psx_light_tunnel(&light, 39));                            /* ... the tunnel's own rows are */
    CHECK(!psx_light_tunnel(&light, 40));
    CHECK_EQ_INT(psx_light_at(&light, 30, 0), PSX_LIGHT_TUNNEL_ROAD);                               /* the CD's tunnel: dim road, half-light wall and foot */
    CHECK_EQ_INT(psx_light_at(&light, 30, 2), PSX_LIGHT_WALL);
    CHECK_EQ_INT(psx_light_at(&light, 30, 4), PSX_LIGHT_WALL);
    CHECK_EQ_INT(psx_light_at(&light, 30, 9), PSX_LIGHT_WALL);
    /* the ramp falls without a step from the open road's 255 to the tunnel's values, on both sides, never back up */
    for (int side = 0; side < 2; side++)
        for (int point = 0; point < 3; point++) {
            const int pts[3] = { 0, 2, 4 };
            int last = PSX_LIGHT_FULL;
            for (int k = PSX_SYNTH_RAMP; k >= 1; k--) {
                const uint32_t node = side == 0 ? (uint32_t)(20 - k) : (uint32_t)(39 + k);
                const int v = psx_light_at(&light, node, pts[point]);
                CHECK(v < last);
                CHECK(v > psx_light_at(&light, 30, pts[point]));
                last = v;
            }
            CHECK(psx_light_at(&light, side == 0 ? 20 : 39, pts[point]) < last);
        }
    CHECK_EQ_INT(psx_light_at(&light, 19, 0), psx_light_at(&light, 40, 0));                          /* the two doors alike */
}

static void test_car_factor_follows_the_ramp(void)
{
    uint8_t property[NODES] = { 0 }, flags[NODES], data[NODES * PSX_LIGHT_POINTS];
    road(property, 20, 39);
    psx_light_synth_tunnels(property, NODES, 0, flags);
    psx_light_synth_rows(flags, NODES, 0, data);
    PsxLight light = { .data = data, .nodes = NODES };
    const float inside = PSX_SYNTH_CAR_PERCENT / 100.0f;
    CHECK(psx_light_synth_car(&light, 5) == 1.0f);
    CHECK(psx_light_synth_car(&light, 30) == inside);
    float last = 1.0f;
    for (int k = PSX_SYNTH_RAMP; k >= 1; k--) {
        const float f = psx_light_synth_car(&light, (uint32_t)(20 - k));
        CHECK(f < last && f > inside);
        last = f;
    }
    CHECK(psx_light_synth_car(NULL, 3) == 1.0f);
}

static void test_tracks(void)
{
    CHECK(psx_light_synth_track("TR6") && psx_light_synth_track("tr7"));
    CHECK(!psx_light_synth_track("TR1") && !psx_light_synth_track("CY1") && !psx_light_synth_track(NULL));
}

int main(void)
{
    test_runs_and_short_runs();
    test_ring_joins_a_run_across_node_zero();
    test_rows_open_ramp_and_tunnel();
    test_car_factor_follows_the_ramp();
    test_tracks();
    return TEST_REPORT();
}
