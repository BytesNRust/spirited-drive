/* The headlight pools (headlight_pool.h): the gain by distance from a car, the ring, several cars, and the light bytes of a PsxLight scaled by it. */
#include "game/race/headlight_pool.h"
#include "game/race/psx_light.h"
#include "tests/test.h"

#include <string.h>

static HeadlightPool pool_of(uint32_t ring, int count, int32_t a, int32_t b)
{
    HeadlightPool p;
    memset(&p, 0, sizeof p);
    p.ring = ring, p.count = count, p.node[0] = a, p.node[1] = b;
    return p;
}

static void test_the_pool_ahead_of_a_car(void)
{
    const HeadlightPool p = pool_of(200, 1, 50, 0);
    CHECK_EQ_INT(headlight_pool_gain(&p, 50), HEADLIGHT_FLOOR);           /* the car's own section is dark */
    CHECK_EQ_INT(headlight_pool_gain(&p, 49), HEADLIGHT_FLOOR);           /* and what is behind it */
    CHECK_EQ_INT(headlight_pool_gain(&p, 50 + HEADLIGHT_START), HEADLIGHT_FLOOR);   /* the light begins ahead */
    CHECK(headlight_pool_gain(&p, 50 + HEADLIGHT_START + 1) > HEADLIGHT_FLOOR);
    CHECK_EQ_INT(headlight_pool_gain(&p, 50 + HEADLIGHT_FULL), 256);      /* full a few nodes ahead */
    int last = 256;
    for (int d = HEADLIGHT_FULL; d <= HEADLIGHT_AHEAD; d++) {              /* never brighter further ahead */
        const int g = headlight_pool_gain(&p, (uint32_t)(50 + d));
        CHECK(g <= last && g >= HEADLIGHT_FLOOR);
        last = g;
    }
    CHECK_EQ_INT(headlight_pool_gain(&p, 50 + HEADLIGHT_AHEAD), HEADLIGHT_FLOOR);   /* the pool ends */
    CHECK_EQ_INT(headlight_pool_gain(&p, 50 + HEADLIGHT_AHEAD + 1), HEADLIGHT_FLOOR);
    CHECK_EQ_INT(headlight_pool_gain(&p, 120), HEADLIGHT_FLOOR);          /* far: the dark's share */
}

static void test_the_pool_slides_with_the_car_inside_a_node(void)
{
    HeadlightPool p = pool_of(200, 1, 50, 0);
    int last = headlight_pool_gain(&p, 56);
    for (int f = 0; f < 256; f += 16) {   /* the car moving on through one node: a node 6 ahead of it rises towards full, in small steps */
        p.sub[0] = (uint8_t)f;
        const int g = headlight_pool_gain(&p, 56);
        CHECK(g >= last && g - last < 40);
        last = g;
    }
    p.sub[0] = 0;
    const int at0 = headlight_pool_gain(&p, 50 + HEADLIGHT_AHEAD - 1);
    p.sub[0] = 255;
    CHECK(headlight_pool_gain(&p, 50 + HEADLIGHT_AHEAD - 1) >= at0);      /* the far end comes nearer */
}

static void test_a_tunnel_is_lit_by_the_beam_too(void)
{
    const HeadlightPool p = pool_of(200, 1, 50, 0);
    CHECK_EQ_INT(headlight_pool_gain_tunnel(&p, 50), TUNNEL_FLOOR);       /* dark but not black */
    CHECK_EQ_INT(headlight_pool_gain_tunnel(&p, 50 + HEADLIGHT_FULL), 256);   /* lit to the whole of its light */
    CHECK(TUNNEL_FLOOR > HEADLIGHT_FLOOR && TUNNEL_FLOOR < 256);
}

static void test_the_roads_own_tunnel_flags_and_the_car_itself(void)
{
    uint8_t flags[200] = { 0 };
    for (int i = 60; i < 80; i++) flags[i] = 1;
    HeadlightPool p = pool_of(200, 1, 50, 0);
    p.tunnel = flags;
    CHECK_EQ_INT(headlight_pool_gain(&p, 120), HEADLIGHT_FLOOR);          /* open: the open dark */
    CHECK_EQ_INT(headlight_pool_gain(&p, 70), TUNNEL_FLOOR);              /* a flagged node beyond the beam: the tunnel's */
    CHECK_EQ_INT(headlight_pool_gain(&p, 50 + HEADLIGHT_FULL), 256);
    CHECK(headlight_pool_car_at(&p, 50) && !headlight_pool_car_at(&p, 51));
    CHECK(!headlight_pool_car_at(NULL, 50));
    CHECK_EQ_INT(headlight_pool_beam(&p, 50), 0);
    CHECK_EQ_INT(headlight_pool_beam(&p, 50 + HEADLIGHT_FULL), 256);
    uint8_t rows[200 * PSX_LIGHT_POINTS];
    memset(rows, 0, sizeof rows);   /* a tunnel whose own rows are black */
    PsxLight light = { .data = rows, .nodes = 200, .pool = &p };
    CHECK_EQ_INT(psx_light_at(&light, 70, 0), 0);                         /* beyond the beam: still black */
    flags[50 + HEADLIGHT_FULL] = 1;
    CHECK_EQ_INT(psx_light_at(&light, 50 + HEADLIGHT_FULL, 0), TUNNEL_BEAM);   /* in the beam: lit */
}

static void test_the_ring_and_several_cars(void)
{
    const HeadlightPool p = pool_of(100, 2, 98, 40);
    const HeadlightPool at_zero = pool_of(100, 1, 0, 0);
    CHECK_EQ_INT(headlight_pool_gain(&p, 3), headlight_pool_gain(&at_zero, 5));   /* node 3 is 5 ahead of 98 across the ring's end: as 5 ahead of 0 */
    CHECK(headlight_pool_gain(&p, 3) > HEADLIGHT_FLOOR);
    CHECK_EQ_INT(headlight_pool_gain(&p, 40 + HEADLIGHT_FULL), 256);                       /* the other car's pool */
    const HeadlightPool none = pool_of(100, 0, 0, 0);
    CHECK_EQ_INT(headlight_pool_gain(&none, 10), HEADLIGHT_FLOOR);        /* no cars: dark */
    CHECK_EQ_INT(headlight_pool_gain(NULL, 10), 256);                     /* no pool: the light as it is */
}

static void test_apply_and_the_light_bytes(void)
{
    CHECK_EQ_INT(headlight_pool_apply(200, 256), 200);
    CHECK_EQ_INT(headlight_pool_apply(200, 128), 100);
    CHECK_EQ_INT(headlight_pool_apply(255, 56), 55);
    uint8_t rows[8 * PSX_LIGHT_POINTS];
    memset(rows, 255, sizeof rows);
    HeadlightPool p = pool_of(8, 1, 1, 0);
    PsxLight light = { .data = rows, .nodes = 8 };
    CHECK_EQ_INT(psx_light_at(&light, 5, 0), 255);                        /* no pool on the light: the file's */
    light.pool = &p;
    CHECK_EQ_INT(psx_light_at(&light, 4, 0), 255);                        /* ahead of the car: lit */
    CHECK_EQ_INT(psx_light_at(&light, 6, 3), headlight_pool_apply(255, HEADLIGHT_FLOOR));   /* far along: dark */
    CHECK_EQ_INT(psx_light_node(&light, 6), headlight_pool_apply(255, HEADLIGHT_FLOOR));
    p.count = 0;                                                          /* the cars gone: all dark */
    CHECK_EQ_INT(psx_light_at(&light, 4, 0), headlight_pool_apply(255, HEADLIGHT_FLOOR));
}

/* the light is ahead of the car's travel (a reversing car lights the lower nodes), between the nodes it glides, the ground beyond the road takes less, a car less than the road */
static void test_travel_glide_width_and_cars(void)
{
    HeadlightPool p = pool_of(200, 1, 50, 0);
    p.dir[0] = -1;
    CHECK(headlight_pool_gain(&p, 45) > headlight_pool_gain(&p, 55));     /* reversing: behind the car in the node order is where the light goes */
    CHECK_EQ_INT(headlight_pool_gain(&p, 47), 256);
    p.dir[0] = 0;                                                         /* unset: forward, as before */
    CHECK(headlight_pool_gain(&p, 55) > headlight_pool_gain(&p, 45));
    int last = headlight_pool_gain(&p, 60);                               /* a car creeping on by a quarter of a node: no step bigger than the lift's own slope */
    for (int sub = 0; sub <= 256; sub += 64) {
        p.sub[0] = (int16_t)sub;
        const int g = headlight_pool_gain(&p, 60);
        CHECK(g >= last - 1);                                             /* moving on: the node 60 is nearer the car, never darker */
        CHECK(g - last < 24);                                             /* and by small steps */
        last = g;
    }
    p.sub[0] = 256;
    HeadlightPool q = pool_of(200, 1, 51, 0);
    CHECK_EQ_INT(headlight_pool_gain(&p, 60), headlight_pool_gain(&q, 60));   /* a whole node of sub is the next node */
    CHECK_EQ_INT(headlight_pool_width(0), 256);
    CHECK(headlight_pool_width(3) < 256 && headlight_pool_width(4) < headlight_pool_width(3) && headlight_pool_width(5) == 0);
    CHECK(headlight_pool_width(8) == headlight_pool_width(3) && headlight_pool_width(10) == 0);
    uint8_t rows[8 * PSX_LIGHT_POINTS];
    memset(rows, 255, sizeof rows);
    HeadlightPool one = pool_of(8, 1, 1, 0);
    PsxLight light = { .data = rows, .nodes = 8 };
    light.pool = &one;
    CHECK_EQ_INT(psx_light_at(&light, 4, 0), 255);
    CHECK_EQ_INT(psx_light_at(&light, 4, 5), headlight_pool_apply(255, HEADLIGHT_FLOOR));   /* the far ground stays dark even in the beam */
    CHECK(psx_light_at(&light, 4, 3) < 255 && psx_light_at(&light, 4, 3) > psx_light_at(&light, 4, 5));
    CHECK_EQ_INT(headlight_pool_car_gain(256), HEADLIGHT_FLOOR + (256 - HEADLIGHT_FLOOR) * HEADLIGHT_CAR_SHARE / 256);
    CHECK(headlight_pool_car_gain(256) < 256);
    CHECK_EQ_INT(headlight_pool_car_gain(HEADLIGHT_FLOOR), HEADLIGHT_FLOOR);   /* the dark stays the dark */
}

/* headlight_cone: the beam is a cone from the car's place along its heading, per point in metres, whatever the track's nodes do */
static HeadlightPool cone_of(double x, double z, double hx, double hz)
{
    HeadlightPool p;
    memset(&p, 0, sizeof p);
    p.cone = 1, p.count = 1;
    p.cone_x[0] = x, p.cone_z[0] = z, p.cone_hx[0] = hx, p.cone_hz[0] = hz;
    return p;
}

static void test_the_cone_follows_the_heading(void)
{
    HeadlightPool p = cone_of(0, 0, 0, 1);                               /* facing +z */
    CHECK_EQ_INT(headlight_pool_lift_xz(&p, 0, 8), 256);                 /* full close ahead */
    CHECK(headlight_pool_lift_xz(&p, 0, 40) > 0 && headlight_pool_lift_xz(&p, 0, 40) < 256);   /* fading with the distance */
    CHECK_EQ_INT(headlight_pool_lift_xz(&p, 0, -30), 0);                 /* nothing far behind */
    CHECK(headlight_pool_lift_xz(&p, 0, -11) > 0 && headlight_pool_lift_xz(&p, 0, -11) < 128);   /* a faint glow just behind the car */
    CHECK_EQ_INT(headlight_pool_lift_xz(&p, 0, 130), 0);                 /* the beam ends */
    CHECK_EQ_INT(headlight_pool_lift_xz(&p, 80, 10), 0);                 /* nor far to the side */
    CHECK(headlight_pool_lift_xz(&p, 3, 8) > 200);                      /* the road's width is lit */
    CHECK(headlight_pool_lift_xz(&p, 10, 25) > 0);                       /* and its verges a little: the cone has no edge but its fading */
    p = cone_of(0, 0, 0, -1);                                            /* the car turned round: the beam went with it */
    CHECK_EQ_INT(headlight_pool_lift_xz(&p, 0, 25), 0);
    CHECK_EQ_INT(headlight_pool_lift_xz(&p, 0, -8), 256);
    p = cone_of(0, 0, 1, 0);                                             /* across the road's nodes' direction: a pool by nodes could not */
    CHECK_EQ_INT(headlight_pool_lift_xz(&p, 8, 0), 256);
    CHECK_EQ_INT(headlight_pool_lift_xz(&p, 0, 25), 0);
}

static void test_the_local_weight(void)
{
    CHECK_EQ_INT(headlight_cone_weight_local(8, 0, 0), 256);
    CHECK_EQ_INT(headlight_cone_weight_local(8, 3, 0), headlight_cone_weight_local(8, -3, 0));   /* symmetric */
    CHECK_EQ_INT(headlight_cone_weight_local(8, 0, 20), 0);                                     /* high above the ground: none */
    CHECK(headlight_cone_weight_local(8, 0, 6) > 0 && headlight_cone_weight_local(8, 0, 6) < 256);
    const HeadlightPool p = cone_of(10, -5, 0.6, 0.8);                                          /* the world's lift is the local one at ground level */
    CHECK_EQ_INT(headlight_pool_lift_xz(&p, 10 + 0.6 * 20, -5 + 0.8 * 20), headlight_cone_weight_local(20, 0, 0));
}

static void test_the_cone_has_no_step(void)
{
    const HeadlightPool p = cone_of(10, -5, 0.6, 0.8);
    int worst = 0;
    for (double along = 0; along < 140; along += 0.5)                     /* walked along the axis and across it: no jump bigger than 16 of 256 per half metre */
        for (double lat = -50; lat < 50; lat += 0.5) {
            const double x = 10 + 0.6 * along - 0.8 * lat, z = -5 + 0.8 * along + 0.6 * lat;
            const double x2 = x + 0.5 * 0.6, z2 = z + 0.5 * 0.8;
            int d = headlight_pool_lift_xz(&p, x, z) - headlight_pool_lift_xz(&p, x2, z2);
            if (d < 0) d = -d;
            if (d > worst) worst = d;
        }
    CHECK(worst <= 16);
    int last = 256;
    for (double along = 30; along < 130; along += 1) {                   /* never brighter further ahead */
        const int l = headlight_pool_lift_xz(&p, 10 + 0.6 * along, -5 + 0.8 * along);
        CHECK(l <= last);
        last = l;
    }
    last = 256;
    for (double lat = 0; lat < 60; lat += 1) {                           /* nor to the side */
        const int l = headlight_pool_lift_xz(&p, 10 + 0.6 * 30 - 0.8 * lat, -5 + 0.8 * 30 + 0.6 * lat);
        CHECK(l <= last);
        last = l;
    }
}

static void test_the_cone_of_several_cars_and_the_old_path(void)
{
    HeadlightPool p = cone_of(0, 0, 0, 1);
    p.count = 2, p.cone_x[1] = 0, p.cone_z[1] = 200, p.cone_hx[1] = 0, p.cone_hz[1] = -1;   /* an oncoming car: both light the road between */
    CHECK_EQ_INT(headlight_pool_lift_xz(&p, 0, 8), 256);   /* ahead of the first, full */
    CHECK(headlight_pool_lift_xz(&p, 0, 150) > 0);        /* ahead of the oncoming one too: the best of the cars */
    CHECK(headlight_pool_lift_xz(&p, 0, 170) > 0);
    p.cone = 0;                                                          /* off: the node path as it was, the node table never read */
    p.ring = 200, p.node[0] = 50, p.node[1] = 0;
    CHECK_EQ_INT(headlight_pool_beam(&p, 50 + HEADLIGHT_FULL), 256);
    CHECK_EQ_INT(headlight_pool_beam(&p, 49), 0);
}

/* the light bytes with the cone: a point's own place decides, the node's row does not (the road runs along z, a car at z = 0 facing +z, then turned round) */
static void test_the_light_bytes_with_the_cone(void)
{
    uint8_t rows[40 * PSX_LIGHT_POINTS];
    memset(rows, 255, sizeof rows);
    int32_t xz[2 * 40];
    for (int i = 0; i < 40; i++) xz[2 * i] = 0, xz[2 * i + 1] = i * 6 * 65536;
    HeadlightPool p = cone_of(0, 0, 0, 1);
    p.ring = 40, p.node_xz = xz;
    PsxLight light = { .data = rows, .nodes = 40, .pool = &p };
    const uint8_t dark = headlight_pool_apply(255, HEADLIGHT_FLOOR);
    const double ahead[2] = { 0, 8 }, behind[2] = { 0, -30 }, wide[2] = { 90, 30 };
    CHECK_EQ_INT(psx_light_at_xz(&light, 5, 0, ahead), 255);               /* ahead of the nose: lit, whatever node it is asked as */
    CHECK_EQ_INT(psx_light_at_xz(&light, 5, 0, behind), dark);             /* behind it: dark */
    CHECK_EQ_INT(psx_light_at_xz(&light, 5, 0, wide), dark);               /* far beside the road: dark */
    CHECK(psx_light_at(&light, 3, 0) > 240);                               /* no place given: the node's centre (18 m on) */
    p.cone_hz[0] = -1;                                                     /* turned round: the same node is dark now, the one behind is lit */
    CHECK_EQ_INT(psx_light_at(&light, 3, 0), dark);
    p.cone = 0;                                                            /* the cone off: the places are ignored, the nodes' pool as it was */
    p.count = 1, p.ring = 40, p.node[0] = 0, p.sub[0] = 0, p.dir[0] = 1;
    CHECK_EQ_INT(psx_light_at_xz(&light, 5, 0, behind), 255);              /* the node 5 ahead along the track is full, the place is not asked */
}

int main(void)
{
    test_the_pool_ahead_of_a_car();
    test_the_pool_slides_with_the_car_inside_a_node();
    test_a_tunnel_is_lit_by_the_beam_too();
    test_the_roads_own_tunnel_flags_and_the_car_itself();
    test_the_ring_and_several_cars();
    test_apply_and_the_light_bytes();
    test_travel_glide_width_and_cars();
    test_the_cone_follows_the_heading();
    test_the_local_weight();
    test_the_cone_has_no_step();
    test_the_cone_of_several_cars_and_the_old_path();
    test_the_light_bytes_with_the_cone();
    return TEST_REPORT();
}
