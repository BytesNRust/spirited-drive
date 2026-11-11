/* port (tunnel_dry): the rain reaches the road at a node by the track's data alone (wet_zones.h), and the grip follows it (race_car_grip): dry inside a closed zone, wet outside, the same for
 * every car at a node. */
#include "game/race/wet_zones.h"
#include "game/race/race_state.h"
#include "game/race/light_synth.h"
#include "game/race/weather.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { NODES = 256, TUNNEL_FROM = 100, TUNNEL_TO = 140 };

static void ring_property(uint8_t *property)
{
    memset(property, 0, NODES);
    for (int i = TUNNEL_FROM; i <= TUNNEL_TO; i++) property[i] = LIGHT_SYNTH_PROPERTY_TUNNEL;
}

static void test_open_by_distance(void)
{
    uint8_t property[NODES];
    ring_property(property);
    uint8_t *open = wet_zones_make(NULL, property, NODES, 1);
    CHECK(open != NULL);
    if (open == NULL) return;
    CHECK_EQ_INT(open[TUNNEL_FROM], 0);
    CHECK_EQ_INT(open[(TUNNEL_FROM + TUNNEL_TO) / 2], 0);
    CHECK_EQ_INT(open[TUNNEL_TO], 0);
    CHECK_EQ_INT(open[10], 16);
    CHECK_EQ_INT(open[200], 16);
    for (int d = 1; d < WEATHER_MOUTH_NODES; d++) {   /* the mouth fades in over the nodes outside, on both sides alike */
        CHECK_EQ_INT(open[TUNNEL_FROM - d], weather_open_for_distance(d));
        CHECK_EQ_INT(open[TUNNEL_TO + d], weather_open_for_distance(d));
    }
    CHECK_EQ_INT(open[TUNNEL_FROM - WEATHER_MOUTH_NODES], 16);
    free(open);
}

/* the places file closes what the road has no tunnel bits for (TR7's steelworks): dry like a tunnel, a canyon over places' tunnel is open air, a reversed track maps its nodes as the light does */
static void test_places_decide(void)
{
    uint8_t property[NODES] = { 0 };
    for (int i = 200; i < 200 + 20; i++) property[i] = LIGHT_SYNTH_PROPERTY_TUNNEL;   /* the road says tunnel here; the places do not */
    TrackNodeLight places = { 0 };
    places.nodes = NODES;
    places.tunnel = calloc(NODES, 1);
    places.canyon = calloc(NODES, 1);
    CHECK(places.tunnel != NULL && places.canyon != NULL);
    if (places.tunnel == NULL || places.canyon == NULL) { track_node_light_free(&places); return; }
    memset(places.tunnel + 30, 1, 11);   /* 30..40: shorter than the road's minimum run, and no road bits */
    uint8_t *open = wet_zones_make(&places, property, NODES, 1);
    CHECK(open != NULL);
    if (open != NULL) {
        CHECK_EQ_INT(open[30], 0);
        CHECK_EQ_INT(open[35], 0);
        CHECK_EQ_INT(open[40], 0);
        CHECK_EQ_INT(open[29], weather_open_for_distance(1));
        CHECK_EQ_INT(open[210], 16);   /* the road's own bits are not added to a places file: it is the only source */
        CHECK_EQ_INT(open[150], 16);
        free(open);
    }
    memset(places.canyon + 34, 1, 3);    /* a canyon is open air even over a tunnel node */
    open = wet_zones_make(&places, property, NODES, 1);
    CHECK(open != NULL);
    if (open != NULL) {
        CHECK_EQ_INT(open[35], weather_open_for_distance(2));   /* 34..36 are open air: the nearest closed nodes are 33 and 37 */
        CHECK_EQ_INT(open[31], 0);
        free(open);
    }
    places.reversed = 1;   /* the file's nodes run the other way: node n is the file's NODES - n */
    open = wet_zones_make(&places, property, NODES, 1);
    CHECK(open != NULL);
    if (open != NULL) {
        CHECK_EQ_INT(open[NODES - 31], 0);
        CHECK_EQ_INT(open[31], 16);
        free(open);
    }
    track_node_light_free(&places);
}

/* Coastal's galleries (property 7): closed by the road's bits when there are no places, by the places' own `tunnel` lines when there are (CL2's file has them) */
static void test_galleries_and_places(void)
{
    uint8_t property[NODES] = { 0 };
    for (int i = 60; i < 60 + 30; i++) property[i] = 7;
    TrackNodeLight places = { 0 };
    places.nodes = NODES;
    places.tunnel = calloc(NODES, 1);
    CHECK(places.tunnel != NULL);
    if (places.tunnel == NULL) return;
    uint8_t *open = wet_zones_make(&places, property, NODES, 1);
    CHECK(open != NULL);
    if (open != NULL) {
        CHECK_EQ_INT(open[75], 16);   /* the places do not list them: dry road is not made */
        free(open);
    }
    memset(places.tunnel + 60, 1, 30);   /* as CL2's file lists them */
    open = wet_zones_make(&places, property, NODES, 1);
    CHECK(open != NULL);
    if (open != NULL) {
        CHECK_EQ_INT(open[75], 0);
        CHECK_EQ_INT(open[150], 16);
        free(open);
    }
    open = wet_zones_make(NULL, property, NODES, 1);   /* no places file: the road's bits */
    CHECK(open != NULL);
    if (open != NULL) {
        CHECK_EQ_INT(open[75], 0);
        free(open);
    }
    track_node_light_free(&places);
}

static void test_short_run_is_not_a_tunnel(void)
{
    uint8_t property[NODES] = { 0 };
    for (int i = 50; i < 50 + LIGHT_SYNTH_MIN_RUN - 1; i++) property[i] = LIGHT_SYNTH_PROPERTY_TUNNEL;
    uint8_t *open = wet_zones_make(NULL, property, NODES, 1);
    CHECK(open != NULL);
    if (open == NULL) return;
    for (int i = 0; i < NODES; i++) CHECK_EQ_INT(open[i], 16);
    free(open);
}

static void test_no_data(void)
{
    CHECK(wet_zones_make(NULL, NULL, NODES, 1) == NULL);
    uint8_t property[NODES] = { 0 };
    CHECK(wet_zones_make(NULL, property, 0, 1) == NULL);
}

static void test_wet_grip_blend(void)
{
    CHECK_EQ_INT(weather_wet_grip(100, 80, 0), 100);
    CHECK_EQ_INT(weather_wet_grip(100, 80, 16), 80);
    CHECK_EQ_INT(weather_wet_grip(100, 80, 8), 90);
    CHECK_EQ_INT(weather_wet_grip(100, 80, -5), 100);
    CHECK_EQ_INT(weather_wet_grip(100, 80, 99), 80);
}

static void test_grip_is_a_function_of_the_node(void)
{
    uint8_t property[NODES];
    ring_property(property);
    uint8_t *open = wet_zones_make(NULL, property, NODES, 1);
    CHECK(open != NULL);
    if (open == NULL) return;
    Tri tri;
    memset(&tri, 0, sizeof tri);
    tri.ring_nodes = NODES;
    RaceState race;
    memset(&race, 0, sizeof race);
    race.tri = &tri;
    race.port_rain_grip = 1;
    race.port_open = open;
    for (int s = 0; s < RACE_SURFACES; s++) race.port_grip_dry[s] = 1000, race.surfaces[s].grip = 800;
    Car player, rival;
    memset(&player, 0, sizeof player);
    memset(&rival, 0, sizeof rival);
    player.surface = rival.surface = 1;
    player.node = rival.node = TUNNEL_FROM + 5;
    CHECK_EQ_INT(race_car_grip(&race, &player), 1000);                 /* inside: dry */
    CHECK_EQ_INT(race_car_grip(&race, &rival), race_car_grip(&race, &player));   /* a rival gets the same at the same node */
    player.node = 10;
    CHECK_EQ_INT(race_car_grip(&race, &player), 800);                  /* outside: wet */
    player.node = TUNNEL_FROM - 3;
    const int32_t mouth = race_car_grip(&race, &player);
    CHECK(mouth > 800 && mouth < 1000);                                /* the mouth is in between */
    player.node = NODES + TUNNEL_FROM + 5;                             /* a node past the ring wraps */
    CHECK_EQ_INT(race_car_grip(&race, &player), 1000);
    race.port_rain_grip = 0;                                           /* off: the surface's own grip */
    CHECK_EQ_INT(race_car_grip(&race, &player), 800);
    race.port_rain_grip = 1;
    race.port_open = NULL;                                             /* no zones: the surface's own grip */
    CHECK_EQ_INT(race_car_grip(&race, &player), 800);
    free(open);
}

int main(void)
{
    test_open_by_distance();
    test_places_decide();
    test_galleries_and_places();
    test_short_run_is_not_a_tunnel();
    test_no_data();
    test_wet_grip_blend();
    test_grip_is_a_function_of_the_node();
    return TEST_REPORT();
}
