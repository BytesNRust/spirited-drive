/* The light made from a road's tunnels (psx_light_synth.h) on a made-up road: which runs are tunnels, the ramp either side, the rows' values as the light
 * reads them, the ring joining a run across node 0. No game or CD data. */
#include "game/race/psx_light_synth.h"
#include "test.h"

#include <math.h>
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

/* CL2's galleries (property 7: the tunnel bit with the open one and the one beside it): not a tunnel as the CD's light is not asked, a tunnel when it is; TR7's open truss (property 5) never is */
static void test_gallery_is_a_tunnel_when_asked(void)
{
    uint8_t property[NODES] = { 0 }, flags[NODES];
    for (int i = 8; i < 8 + PSX_SYNTH_MIN_RUN; i++) property[i] = 7;
    for (int i = 40; i < 40 + PSX_SYNTH_MIN_RUN; i++) property[i] = 5;
    psx_light_synth_tunnels(property, NODES, 0, flags);
    CHECK_EQ_INT(flags[8], 0);
    psx_light_synth_tunnels_ex(property, NODES, 0, 0, flags);
    CHECK_EQ_INT(flags[8], 0);
    psx_light_synth_tunnels_ex(property, NODES, 0, 1, flags);
    CHECK_EQ_INT(flags[8], PSX_SYNTH_FLAG_TUNNEL);
    CHECK_EQ_INT(flags[8 + PSX_SYNTH_MIN_RUN - 1], PSX_SYNTH_FLAG_TUNNEL);
    CHECK_EQ_INT(flags[8 + PSX_SYNTH_MIN_RUN], 0);
    CHECK_EQ_INT(flags[40], 0);   /* the truss stays open */
    CHECK(psx_light_synth_gallery_track("CL2") && psx_light_synth_gallery_track("cl2") && !psx_light_synth_gallery_track("CL1") && !psx_light_synth_gallery_track(NULL));
}

/* the City's tunnels' car ratio from the CD's LGT: CY3's lamps are orange (the blue channel a third of the red), CY1's are grey and brighter at night; the car factor follows the node's parity and the light's own */
static void test_warm_tunnel_tracks(void)
{
    CHECK(psx_light_synth_city_track("CY1") == 1 && psx_light_synth_city_track("cy2") == 2 && psx_light_synth_city_track("CY3") == 2);
    CHECK(psx_light_synth_city_track("TR3") == 3);
    CHECK(psx_light_synth_city_track("TR6") == 0 && psx_light_synth_city_track("TR7") == 0 && psx_light_synth_city_track("CL2") == 0 && psx_light_synth_city_track(NULL) == 0);
}

/* port (tunnel look): the shadow's remap is made in the shown palette in a tunnel of a light with a look, and only there */
static void test_look_tunnel(void)
{
    enum { N = 64 };
    uint8_t tunnel[N] = { 0 }, data[N * PSX_LIGHT_POINTS];
    memset(tunnel + 20, 1, 21);
    memset(data, 255, sizeof data);
    PsxLight light = { 0 };
    light.nodes = N, light.tunnel = tunnel, light.data = data;
    CHECK(!psx_light_look_tunnel(&light, 30));   /* no look: the art palette's remap is right */
    light.look_set = 1;
    CHECK(psx_light_look_tunnel(&light, 30) && psx_light_look_tunnel(&light, 20) && psx_light_look_tunnel(&light, 40));
    CHECK(!psx_light_look_tunnel(&light, 19) && !psx_light_look_tunnel(&light, 41));   /* the open road */
    CHECK(!psx_light_look_tunnel(NULL, 30));
}

/* port (car_tunnel_match): the car's light in a tunnel with a look against the world's at the same tunnel_tone. The rows give a dimmed road (100 / 255) in the tunnel; level 0 is the 7-Nov
 * formula, 1 divides by the night's tint mean, takes full light's share (1.0) in place of the road's and keeps the lamp alternation soft: 1.0 under a lamp (even node), 0.75 between two (odd) */
static void test_car_tunnel_match(void)
{
    enum { N = 64 };
    uint8_t tunnel[N] = { 0 }, data[N * PSX_LIGHT_POINTS];
    memset(tunnel + 20, 1, 21);   /* nodes 20..40 */
    memset(data, 255, sizeof data);
    PsxLight light = { 0 };
    light.nodes = N, light.tunnel = tunnel, light.data = data, light.look_set = 1, light.look_light = 100.0;
    light.look_cast[0] = light.look_cast[1] = light.look_cast[2] = 1.0f;
    const float road = 100.0f / 255.0f, mean = 0.5f, dark_gap = 0.75f;
    float out[3], old[3];
    psx_light_synth_car_tunnel(&light, 30, old);
    CHECK(fabsf(old[0] - road) < 1e-5f);   /* the 7-Nov car: tunnel_tone x the road's share */
    for (uint32_t node = 30; node <= 31; node++) {
        psx_light_synth_car_tunnel_match(&light, node, 0, mean, out);
        psx_light_synth_car_tunnel(&light, node, old);
        CHECK(memcmp(out, old, sizeof out) == 0);   /* level 0 is that, whatever the tint */
        psx_light_synth_car_tunnel_match(&light, node, 1, mean, out);   /* the world's gain, full light's share, the soft alternation: 1.0 under a lamp, 0.75 between */
        const float gap = (node & 1u) ? dark_gap : 1.0f;
        CHECK(fabsf(out[0] - gap / mean) < 1e-4f && fabsf(out[1] - out[0]) < 1e-6f);
    }
    light.look_light = 300.0;   /* above full light: 3 x, the excess coming in with the road's darkening as the world's does */
    psx_light_synth_car_tunnel_match(&light, 30, 1, mean, out);
    CHECK(fabsf(out[0] - 3.0f / mean) < 1e-3f);
    psx_light_synth_car_tunnel_match(&light, 31, 1, mean, out);
    CHECK(fabsf(out[0] - 3.0f * dark_gap / mean) < 1e-3f);
    psx_light_synth_car_tunnel_match(&light, 30, 1, 0.0f, out);   /* no tint to divide by: none divided */
    CHECK(fabsf(out[0] - 3.0f) < 1e-3f);
    psx_light_synth_car_tunnel_match(&light, 10, 1, mean, out);   /* the open road and the far side are 1 */
    CHECK(out[0] == 1.0f && out[1] == 1.0f && out[2] == 1.0f);
    psx_light_synth_car_tunnel_match(&light, 50, 1, mean, out);
    CHECK(out[0] == 1.0f && out[1] == 1.0f && out[2] == 1.0f);
    light.look_light = 100.0;
    psx_light_synth_car_tunnel_match(&light, 18, 1, mean, out);   /* the ramp outside the mouth (the rows there are the open road's): no brighter than the open road */
    CHECK(out[0] <= 1.0f + 1e-5f);
}

/* port (tunnel zone looks): in every tunnel the car takes its zone's light and colour: under a lamp (even node) light% / 100 x colour (capped at 1 here: these lights have no rows, so no road darkening for the excess to come in with; test_car_above_full_light_takes_the_excess), between two lamps (odd) the gap's 40 % of that,
 * coming in over the weather's 6 nodes outside the mouth; a zone with a look of its own (a tone slot) gives its own */
static void test_car_takes_the_zones_look(void)
{
    enum { N = 64 };
    uint8_t tunnel[N] = { 0 }, tone_id[N] = { 0 };
    uint16_t tone_mask[N] = { 0 };
    memset(tunnel + 20, 1, 21);   /* nodes 20..40 */
    PsxLight light = { 0 };
    light.nodes = N, light.tunnel = tunnel, light.look_set = 1, light.look_light = 104.4;
    light.look_cast[0] = 1.0f, light.look_cast[1] = 0.35f, light.look_cast[2] = 0.05f;
    float out[3];
    psx_light_synth_car_tunnel(&light, 30, out);   /* under a lamp: 104.4 % x (1, .35, .05), capped at 1 */
    CHECK(fabsf(out[0] - 1.0f) < 1e-5f && fabsf(out[1] - 0.3654f) < 1e-4f && fabsf(out[2] - 0.0522f) < 1e-4f);
    psx_light_synth_car_tunnel(&light, 31, out);   /* between: 40 % of it */
    CHECK(fabsf(out[0] - 0.4f) < 1e-5f && fabsf(out[1] - 0.14616f) < 1e-4f && fabsf(out[2] - 0.02088f) < 1e-4f);
    psx_light_synth_car_tunnel(&light, 18, out);   /* 2 nodes outside the mouth, a lamp node: dark 1 - 2/6 of the way in */
    CHECK(fabsf(out[1] - (1.0f - (1.0f - 0.3654f) * (1.0f - 2.0f / 6.0f))) < 1e-4f);
    psx_light_synth_car_tunnel(&light, 14, out);   /* 6 nodes out: the open road */
    CHECK(out[0] == 1.0f && out[1] == 1.0f && out[2] == 1.0f);
    psx_light_synth_car_tunnel(&light, 47, out);   /* and past the other mouth */
    CHECK(out[0] == 1.0f && out[1] == 1.0f && out[2] == 1.0f);
    light.look_light = 50.0, light.look_cast[1] = light.look_cast[2] = 1.0f;   /* a white 50 % tunnel: half the light, even and gap */
    psx_light_synth_car_tunnel(&light, 30, out);
    CHECK(fabsf(out[0] - 0.5f) < 1e-5f && fabsf(out[1] - 0.5f) < 1e-5f);
    psx_light_synth_car_tunnel(&light, 31, out);
    CHECK(fabsf(out[2] - 0.2f) < 1e-5f);
    /* a zone with its own look: nodes 20..30 through a slot (150 %, red), the rest the time's */
    light.tone_count = 1, light.tone[0].look = 1, light.tone[0].light = 150.0f;
    light.tone[0].cast[0] = 1.0f, light.tone[0].cast[1] = 0.5f, light.tone[0].cast[2] = 0.0f;
    light.tone_id = tone_id, light.tone_mask = tone_mask;
    memset(tone_id + 20, 1, 11);
    psx_light_synth_car_tunnel(&light, 24, out);
    CHECK(fabsf(out[0] - 1.0f) < 1e-5f && fabsf(out[1] - 0.75f) < 1e-5f && fabsf(out[2]) < 1e-5f);
    psx_light_synth_car_tunnel(&light, 36, out);   /* the other zone: the time's look */
    CHECK(fabsf(out[0] - 0.5f) < 1e-5f);
    psx_light_synth_car_tunnel(&light, 19, out);   /* one node before the first zone: its look, ramping */
    CHECK(fabsf(out[1] - (1.0f - (1.0f - 0.4f * 0.75f) * (1.0f - 1.0f / 6.0f))) < 1e-4f);
    uint8_t hall[N] = { 0 };   /* a hall keeps its lamps' own factors */
    hall[35] = 1, light.hall = hall;
    psx_light_synth_car_tunnel(&light, 35, out);
    CHECK(out[0] > out[1] && out[1] > out[2]);
}

/* the car is never brighter than the road around it: its zone's look is taken over the light the road gets at its node (the rows' road points, a tunnel's road capped as the world's: TR6's, CL2's and
 * CY1's rows say 255 and the world shows 100 of 255) */
static void test_car_is_no_brighter_than_the_road(void)
{
    enum { N = 64 };
    uint8_t tunnel[N] = { 0 }, data[N * PSX_LIGHT_POINTS];
    memset(tunnel + 20, 1, 21);
    for (int n = 0; n < N; n++) {   /* the rows of TR6's tunnel: the road 255, the foot 130, the walls 0 (the world takes the walls to 128 and the road to 100); the open road's all 255 */
        static const uint8_t ROW[PSX_LIGHT_POINTS] = { 255, 255, 130, 0, 0, 0, 255, 130, 0, 0, 0 };
        if (tunnel[n]) memcpy(data + (size_t)n * PSX_LIGHT_POINTS, ROW, PSX_LIGHT_POINTS);
        else memset(data + (size_t)n * PSX_LIGHT_POINTS, 255, PSX_LIGHT_POINTS);
    }
    PsxLight light = { 0 };
    light.nodes = N, light.data = data, light.tunnel = tunnel, light.look_set = 1, light.look_light = 100.0;
    light.look_cast[0] = light.look_cast[1] = light.look_cast[2] = 1.0f;
    float out[3];
    psx_light_synth_car_tunnel(&light, 30, out);   /* under a lamp: the road's 100 of 255, not the look's full */
    CHECK(fabsf(out[0] - 100.0f / 255.0f) < 1e-4f && out[0] == out[1] && out[1] == out[2]);
    psx_light_synth_car_tunnel(&light, 31, out);   /* between: 40 % of that */
    CHECK(fabsf(out[0] - 0.4f * 100.0f / 255.0f) < 1e-4f);
    float road = psx_light_road_light(&light, 30);
    CHECK(fabsf(road - 100.0f / 255.0f) < 1e-4f);
    CHECK(psx_light_road_light(&light, 5) == 1.0f);   /* the open road's rows: the road points at 255 */
    light.look_light = 50.0, light.look_cast[1] = 0.5f;   /* a dimmer, orange zone: its look, then the road's share of it */
    psx_light_synth_car_tunnel(&light, 30, out);
    CHECK(fabsf(out[0] - 0.5f * 100.0f / 255.0f) < 1e-4f && fabsf(out[1] - 0.25f * 100.0f / 255.0f) < 1e-4f);
    CHECK(out[0] <= road && out[1] <= road && out[2] <= road);
    psx_light_synth_car_tunnel(&light, 14, out);   /* 6 nodes from the mouth: the open road */
    CHECK(out[0] == 1.0f);
}

/* port (tunnel_tone, the car above full light): a tunnel look brighter than 100 % lights the world by its excess (the walls and the road are 1.87 times brighter at 215 % than at 115 %), and the car takes it
 * too: the look's light x colour is no longer capped at 1 before the road's share, so the car is lit as its surroundings are. The excess comes in with the road's darkening (the rows' road at 100 of 255 in a
 * tunnel, 255 outside), so the open road before a mouth is not brightened; at 100 % and white nothing is different. */
static void test_car_above_full_light_takes_the_excess(void)
{
    enum { N = 64 };
    uint8_t tunnel[N] = { 0 }, data[N * PSX_LIGHT_POINTS];
    memset(tunnel + 20, 1, 21);
    for (int n = 0; n < N; n++) {
        static const uint8_t ROW[PSX_LIGHT_POINTS] = { 255, 255, 130, 0, 0, 0, 255, 130, 0, 0, 0 };
        if (tunnel[n]) memcpy(data + (size_t)n * PSX_LIGHT_POINTS, ROW, PSX_LIGHT_POINTS);
        else memset(data + (size_t)n * PSX_LIGHT_POINTS, 255, PSX_LIGHT_POINTS);
    }
    PsxLight light = { 0 };
    light.nodes = N, light.data = data, light.tunnel = tunnel, light.look_set = 1, light.look_light = 215.0;
    light.look_cast[0] = 1.0f, light.look_cast[1] = 0.68f, light.look_cast[2] = 0.28f;   /* CY1's tunnel_tone 215 1/0.68/0.28 */
    const float road = 100.0f / 255.0f;
    float out[3], at115[3];
    psx_light_synth_car_tunnel(&light, 30, out);   /* under a lamp: the look's own light x colour (2.15, 1.462, .602) x the road's share */
    CHECK(fabsf(out[0] - 2.15f * road) < 1e-4f && fabsf(out[1] - 1.462f * road) < 1e-4f && fabsf(out[2] - 0.602f * road) < 1e-4f);
    light.look_light = 115.0;
    psx_light_synth_car_tunnel(&light, 30, at115);
    CHECK(fabsf(at115[0] - 1.15f * road) < 1e-4f && fabsf(at115[1] - 0.782f * road) < 1e-4f);
    CHECK(fabsf(out[0] / at115[0] - 2.15f / 1.15f) < 1e-3f);   /* the car goes up with the look as the world does */
    light.look_light = 215.0;
    psx_light_synth_car_tunnel(&light, 31, out);   /* between two lamps: 40 % of it, the flicker stays */
    CHECK(fabsf(out[0] - 0.4f * 2.15f * road) < 1e-4f);
    psx_light_synth_car_tunnel(&light, 16, out);   /* 4 nodes out, the road still open: no excess, the old ratio (the amber tint of the mouth only) */
    CHECK(out[0] == 1.0f && out[1] == 1.0f && fabsf(out[2] - (1.0f - (1.0f - 0.602f) * (1.0f - 4.0f / 6.0f))) < 1e-4f);
    light.look_light = 100.0, light.look_cast[0] = light.look_cast[1] = light.look_cast[2] = 1.0f;   /* 100 % and white: as it was */
    psx_light_synth_car_tunnel(&light, 30, out);
    CHECK(fabsf(out[0] - road) < 1e-5f && out[0] == out[1] && out[1] == out[2]);
}

/* a reversed track: the file's arrays (tunnel, hall, car_dim) are of its own direction; the car asks them at the node the file has (psx_light_old_node), not the game's: file node n is game node (nodes - n) */
static void test_a_reversed_track_has_its_places_mirrored_for_the_car(void)
{
    enum { N = 64 };
    uint8_t tunnel[N] = { 0 }, hall[N] = { 0 }, dim[N] = { 0 };
    memset(tunnel + 10, 1, 11);   /* the file's 10..20, the game's 44..54 */
    memset(hall + 40, 1, 3);      /* the file's 40..42, the game's 22..24 */
    memset(dim + 50, 1, 3);       /* the file's 50..52, the game's 12..14 */
    PsxLight light = { 0 };
    light.nodes = N, light.tunnel = tunnel, light.hall = hall, light.car_dim = dim, light.reversed = 1;
    float out[3];
    psx_light_synth_car_tunnel(&light, 48, out);   /* the file's 16: in the tunnel (even node, under a lamp) */
    CHECK(out[0] < 0.7f);
    psx_light_synth_car_tunnel(&light, 16, out);   /* the file's 48: open road, whatever the game's node 16 would be unmirrored */
    CHECK(out[0] == 1.0f && out[1] == 1.0f && out[2] == 1.0f);
    psx_light_synth_car_tunnel(&light, 24, out);   /* the file's 40: the hall's lamps, warm */
    CHECK(out[0] > 0.9f && out[0] > out[1] && out[1] > out[2]);
    psx_light_synth_car_tunnel(&light, 40, out);   /* the file's 24: not the hall */
    CHECK(out[0] == 1.0f);
    psx_light_synth_car_tunnel(&light, 13, out);   /* the file's 51: the car keeps its dim tone */
    CHECK(fabsf(out[0] - 62.0f / 160.0f) < 1e-5f);
    psx_light_synth_car_tunnel(&light, 50, out);   /* the file's 14 (even: under a lamp): not the dim one but the tunnel's */
    CHECK(fabsf(out[0] - 62.0f / 160.0f) > 1e-3f);
    uint8_t flags[N] = { 0 };   /* the same light with no tunnel array: psx_light_synth_car's own hall lookup */
    light.tunnel = NULL;
    psx_light_synth_car(&light, 24, out);   /* the file's 40, a hall node */
    CHECK(out[0] > 0.9f && out[0] > out[1]);
    psx_light_synth_car(&light, 41, out);   /* the file's 23 */
    CHECK(out[0] == 1.0f);
    (void)flags;
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
    CHECK(lit[0] > lit[1] && lit[1] > lit[2] && dim[0] > dim[1] && dim[1] > dim[2]);   /* warm orange, both under and between the lamps */
    CHECK(fabsf(lit[0] - 150.0f / 160) < 1e-5f && fabsf(lit[2] - 32.0f / 160) < 1e-5f);
    /* the made light: the terrain's lower light took the mark's byte, the node's flag carries the hall */
    uint8_t made[NODES * PSX_LIGHT_POINTS], hall[NODES] = { 0 };
    memset(made, 61, sizeof made);   /* the night's terrain light at the start straight */
    hall[20] = hall[21] = 1;
    PsxLight own = { 0 };
    own.data = made, own.nodes = NODES, own.hall = hall;
    float flagged[3], other[3];
    psx_light_synth_car(&own, 20, flagged);
    psx_light_synth_car(&own, 30, other);
    CHECK(fabsf(flagged[0] - 150.0f / 160) < 1e-5f && fabsf(flagged[2] - 32.0f / 160) < 1e-5f);   /* the warm alternation, whatever the byte says */
    CHECK(other[0] < 1.0f && other[0] != flagged[0]);                                              /* a node not flagged takes the tunnel formula */
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
    uint32_t from = 0, to = 0;
    CHECK(psx_light_synth_hall("TR7", &from, &to) && from == 471 && to == 570 && psx_light_synth_hall("tr7", NULL, NULL));   /* the foundry hall */
    CHECK(!psx_light_synth_hall("TR6", &from, &to) && !psx_light_synth_hall(NULL, &from, &to));
    CHECK(!psx_light_synth_track("TR1") && !psx_light_synth_track("CY1") && !psx_light_synth_track(NULL));
}

/* the night's tunnel: the road and the wall foot at the open road's 255 (the night's tint is what darkens it; the dusk's 214 and 106 would leave it black),
 * the points beyond the wall dark as at every time, and still a tunnel to the renderer */
static void test_night_tunnel_is_not_darker_than_the_road(void)
{
    uint8_t data[NODES * PSX_LIGHT_POINTS];
    PsxLight light = { 0 };
    rows(&light, data, PSX_SYNTH_NIGHT);
    CHECK_EQ_INT(psx_light_byte(&light, 30, 0), PSX_LIGHT_FULL);   /* the road */
    CHECK_EQ_INT(psx_light_byte(&light, 30, 2), PSX_LIGHT_FULL);   /* the wall foot */
    CHECK_EQ_INT(psx_light_byte(&light, 30, 3), 0);                /* beyond it */
    CHECK(psx_light_tunnel(&light, 30));
    CHECK_EQ_INT(psx_light_byte(&light, 5, 0), PSX_LIGHT_FULL);
}

/* a made light's own tunnel flags (made_tunnels): the nodes the road marks are tunnels whatever the rows say, the rest are not */
static void test_light_tunnel_flags_override_the_rows(void)
{
    uint8_t data[NODES * PSX_LIGHT_POINTS], flags[NODES] = { 0 };
    memset(data, PSX_LIGHT_FULL, sizeof data);   /* a night's rows: 255 everywhere, no row a tunnel's */
    for (int i = 20; i < 30; i++) flags[i] = 1;
    PsxLight light = { .data = data, .nodes = NODES };
    CHECK(!psx_light_tunnel(&light, 25));
    light.tunnel = flags;
    CHECK(psx_light_tunnel(&light, 25) && psx_light_tunnel(&light, 20) && psx_light_tunnel(&light, 29));
    CHECK(!psx_light_tunnel(&light, 19) && !psx_light_tunnel(&light, 30));
}

/* the made light's selection (made_tunnels): a road tunnel takes the tunnel's rows and its flag, a hall (its own flag, or hall_light's range) keeps the terrain's rows and no flag */
static void test_made_light_selection_hall_wins(void)
{
    enum { N = 40 };
    uint8_t made_data[N * PSX_LIGHT_POINTS], tunnel_data[N * PSX_LIGHT_POINTS], hall[N] = { 0 }, flags[N] = { 0 };
    memset(made_data, 20, sizeof made_data);
    memset(tunnel_data, 200, sizeof tunnel_data);
    for (int i = 5; i < 35; i++) flags[i] = 1;     /* the road's tunnel run */
    for (int i = 8; i < 12; i++) hall[i] = 1;      /* a hall inside it (property 16 as well) */
    PsxLight made = { .data = made_data, .nodes = N, .hall = hall };
    PsxLight tunnels = { .data = tunnel_data, .nodes = N, .tunnel = flags };
    psx_light_synth_made_tunnels(&made, &tunnels, 20, 24);   /* hall_light's range */
    CHECK(made.tunnel == flags && tunnels.tunnel == NULL);
    CHECK(psx_light_tunnel(&made, 6) && psx_light_tunnel(&made, 30));
    CHECK_EQ_INT(psx_light_byte(&made, 6, 3), 200);       /* a tunnel's rows */
    CHECK(!psx_light_tunnel(&made, 9));                   /* the hall flag wins ... */
    CHECK_EQ_INT(psx_light_byte(&made, 9, 3), 20);        /* ... and the terrain's rows stay */
    CHECK(!psx_light_tunnel(&made, 22));                  /* hall_light's range wins too */
    CHECK_EQ_INT(psx_light_byte(&made, 22, 3), 20);
    CHECK_EQ_INT(psx_light_byte(&made, 2, 3), 20);        /* the open road is untouched */
}

/* car_tunnel_light: the car's darkness is the road's tunnel flags': a canyon's shade in the rows (point 4 low, no flag) leaves it at 1, the tunnel is the lamp alternation, and the
 * door ramps back over PSX_SYNTH_CAR_RAMP nodes where the rows took ten */
static void test_car_tunnel_light_follows_the_flags(void)
{
    enum { N = 60 };
    uint8_t data[N * PSX_LIGHT_POINTS], flags[N] = { 0 };
    memset(data, PSX_LIGHT_FULL, sizeof data);
    for (int i = 40; i < 50; i++) data[i * PSX_LIGHT_POINTS + PSX_SYNTH_HALL_POINT] = 60;   /* a canyon: shaded rows, no tunnel */
    for (int i = 10; i < 20; i++) flags[i] = 1;
    PsxLight light = { .data = data, .nodes = N, .tunnel = flags };
    float open[3], old[3], in_even[3], in_odd[3], door1[3], door2[3], past[3];
    psx_light_synth_car_tunnel(&light, 44, open);
    psx_light_synth_car(&light, 44, old);
    CHECK(old[0] < 0.95f);                                   /* the rows alone darkened the car there ... */
    for (int k = 0; k < 3; k++) CHECK(fabsf(open[k] - 1.0f) < 1e-6f);   /* ... the flags do not */
    psx_light_synth_car_tunnel(&light, 14, in_even);
    psx_light_synth_car_tunnel(&light, 15, in_odd);
    CHECK(in_even[0] < 0.7f && in_odd[0] < in_even[0]);      /* the lamp alternation inside */
    psx_light_synth_car_tunnel(&light, 20, door1);           /* one node past the last one */
    psx_light_synth_car_tunnel(&light, 22, door2);
    psx_light_synth_car_tunnel(&light, 24, past);
    CHECK(door1[1] > in_even[1] || door1[1] > in_odd[1]);
    CHECK(door2[0] > door1[0] || door2[1] > door1[1]);
    for (int k = 0; k < 3; k++) CHECK(fabsf(past[k] - 1.0f) < 1e-6f);   /* the open road's light 3 nodes out */
    light.tunnel = NULL;                                     /* no flags: as psx_light_synth_car */
    float same[3];
    psx_light_synth_car_tunnel(&light, 44, same);
    for (int k = 0; k < 3; k++) CHECK(fabsf(same[k] - old[k]) < 1e-6f);
}

static void test_mine_track(void)
{
    CHECK(psx_light_synth_mine("TR6") && psx_light_synth_mine("tr6"));
    CHECK(!psx_light_synth_mine("TR7") && !psx_light_synth_mine(NULL));
}

static void test_trans_closed_range(void)
{
    uint32_t a = 0, b = 0, r = 0;
    CHECK(psx_light_synth_trans_closed("TR7", &a, &b, &r));
    CHECK(a == 465 && b == 470 && r == 475);   /* just before the marked run, which starts at 471 */
    CHECK(!psx_light_synth_trans_closed("TR6", &a, &b, &r) && !psx_light_synth_trans_closed(NULL, &a, &b, &r));
}

static void test_parking_dark(void)
{
    uint32_t a = 0, b = 0;
    CHECK(psx_light_synth_parking("TR7", &a, &b));
    CHECK(a == 465 && b == 570);   /* the deck's first node to the hall's end, the exit tunnel's start is 571 */
    CHECK(!psx_light_synth_parking("TR6", &a, &b) && !psx_light_synth_parking(NULL, &a, &b));
    uint8_t dim[8] = { 0, 1, 1, 1, 0, 0, 0, 0 }, tunnel[8] = { 0 };
    PsxLight light = { 0 };
    light.nodes = 8, light.car_dim = dim, light.tunnel = tunnel;
    float even[3], odd[3];
    psx_light_synth_car_tunnel(&light, 2, even);
    psx_light_synth_car_tunnel(&light, 3, odd);
    CHECK(even[0] == odd[0] && even[1] == odd[1] && even[2] == odd[2]);   /* no alternation inside the span */
    CHECK(even[0] < 0.4f && even[2] < 0.4f);   /* the alternation's dim tone, 62/160 */
    psx_light_synth_car_tunnel(&light, 4, even);
    psx_light_synth_car_tunnel(&light, 5, odd);
    CHECK(even[0] == 1.0f && odd[0] == 1.0f);   /* outside it, no tunnel: the open light */
}

static void test_cavern_range(void)
{
    uint32_t a = 0, b = 0;
    CHECK(psx_light_synth_cavern("TR6", &a, &b));
    CHECK(a == 322 && b == 546);   /* after the mine tunnel's mouth, to the tunnel run's end */
    CHECK(!psx_light_synth_cavern("TR7", &a, &b) && !psx_light_synth_cavern(NULL, &a, &b));
}

static void test_trans_ranges(void)
{
    uint32_t a = 0, b = 0;
    CHECK(psx_light_synth_trans("tr7", &a, &b));
    CHECK(a == 700 && b == 776);
    CHECK(!psx_light_synth_trans("TR6", &a, &b) && !psx_light_synth_trans(NULL, &a, &b));
}

int main(void)
{
    test_trans_ranges();
    test_trans_closed_range();
    test_parking_dark();
    test_cavern_range();
    test_mine_track();
    test_runs_and_short_runs();
    test_hall_is_a_tunnel();
    test_gallery_is_a_tunnel_when_asked();
    test_warm_tunnel_tracks();
    test_car_takes_the_zones_look();
    test_look_tunnel();
    test_car_tunnel_match();
    test_car_is_no_brighter_than_the_road();
    test_car_above_full_light_takes_the_excess();
    test_a_reversed_track_has_its_places_mirrored_for_the_car();
    test_hall_rows_and_cars();
    test_ring_joins_a_run_across_node_zero();
    test_rows_open_and_tunnel();
    test_doors_follow_the_measured_ramps();
    test_times_of_day();
    test_night_tunnel_is_not_darker_than_the_road();
    test_car_factor_follows_the_beyond_points();
    test_lamps_flicker_node_by_node();
    test_tracks();
    test_light_tunnel_flags_override_the_rows();
    test_made_light_selection_hall_wins();
    test_car_tunnel_light_follows_the_flags();
    return TEST_REPORT();
}
