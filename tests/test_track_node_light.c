/* The light effects' baked light (track_node_light.h) on made-up rows: the header's checks, the node and point lookups, the file's
 * garbage last node. No game or CD data. */
#include "game/race/track_node_light.h"
#include "game/race/psx_lgt.h"
#include "test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { NODES = 6, TRAILER = 12 };

static size_t make_file(uint8_t *out, uint32_t nodes, uint32_t header_nodes)
{
    memset(out, 0, 8 + (size_t)nodes * TRACK_NODE_LIGHT_POINTS + TRAILER);
    out[0] = (uint8_t)header_nodes, out[4] = 0;
    out[5] = 1;   /* scale 256 */
    for (uint32_t n = 0; n < nodes; n++)
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) out[8 + n * TRACK_NODE_LIGHT_POINTS + (uint32_t)p] = (uint8_t)(16 * n + p);
    memcpy(out + 8 + (size_t)nodes * TRACK_NODE_LIGHT_POINTS, "CRCF", 4);
    return 8 + (size_t)nodes * TRACK_NODE_LIGHT_POINTS + TRAILER;
}

static void test_object_floor_lifts_only_the_points_beyond_the_road(void)
{
    uint8_t file[8 + NODES * TRACK_NODE_LIGHT_POINTS + TRAILER];
    const size_t size = make_file(file, NODES, NODES);
    TrackNodeLight light = { 0 };
    CHECK(psx_lgt_parse(file, size, NODES, &light));
    light.object_floor = 245;   /* port (vegas_objects) */
    CHECK_EQ_INT(track_node_light_at(&light, 1, 0), 16);     /* the road's own points keep their dark */
    CHECK_EQ_INT(track_node_light_at(&light, 1, 6), 16 + 6);
    CHECK_EQ_INT(track_node_light_at(&light, 1, 2), 16 + 2);
    CHECK_EQ_INT(track_node_light_at(&light, 1, 3), 245);   /* the buildings' side takes the floor */
    CHECK_EQ_INT(track_node_light_at(&light, 1, 9), 245);
    light.object_floor = 0;
    CHECK_EQ_INT(track_node_light_at(&light, 1, 9), 16 + 9);   /* off: as it was */
    track_node_light_free(&light);
}

static void test_parse_and_lookup(void)
{
    uint8_t file[8 + NODES * TRACK_NODE_LIGHT_POINTS + TRAILER];
    const size_t size = make_file(file, NODES, NODES);
    TrackNodeLight light = { 0 };
    CHECK(psx_lgt_parse(file, size, NODES, &light));
    CHECK_EQ_INT(light.nodes, NODES);
    CHECK_EQ_INT(track_node_light_at(&light, 0, 0), 0);
    CHECK_EQ_INT(track_node_light_at(&light, 2, 7), 16 * 2 + 7);
    CHECK_EQ_INT(track_node_light_at(&light, 2 + NODES, 7), 16 * 2 + 7);           /* the ring wraps */
    CHECK_EQ_INT(track_node_light_at(&light, NODES - 1, 3), 16 * (NODES - 2) + 3);   /* the last node is not light */
    CHECK_EQ_INT(track_node_light_at(&light, 1, -1), TRACK_NODE_LIGHT_FULL);
    CHECK_EQ_INT(track_node_light_at(&light, 1, TRACK_NODE_LIGHT_POINTS), TRACK_NODE_LIGHT_FULL);
    CHECK_EQ_INT(track_node_light_at(NULL, 1, 1), TRACK_NODE_LIGHT_FULL);
    CHECK_EQ_INT(track_node_light_node(&light, 2), 38);   /* the terrain points' mean (32 + 2..5, 7..10), the road's left out */
    CHECK_EQ_INT(track_node_light_node(NULL, 2), TRACK_NODE_LIGHT_FULL);
    track_node_light_free(&light);
    CHECK(light.data == NULL);
    CHECK_EQ_INT(track_node_light_at(&light, 1, 1), TRACK_NODE_LIGHT_FULL);
    CHECK_EQ_INT(track_node_light_node(&light, 1), TRACK_NODE_LIGHT_FULL);
}

static void test_rejected_files(void)
{
    uint8_t file[8 + NODES * TRACK_NODE_LIGHT_POINTS + TRAILER];
    TrackNodeLight light = { 0 };
    const size_t size = make_file(file, NODES, NODES);
    CHECK(!psx_lgt_parse(file, size, NODES + 1, &light));                     /* another track's node count */
    CHECK(!psx_lgt_parse(file, 8 + (NODES - 1) * TRACK_NODE_LIGHT_POINTS, NODES, &light));   /* rows cut short */
    CHECK(!psx_lgt_parse(file, 4, NODES, &light));
    CHECK(!psx_lgt_parse(NULL, size, NODES, &light));
    make_file(file, NODES, NODES + 1);
    CHECK(!psx_lgt_parse(file, size, NODES, &light));                         /* the header says otherwise */
    CHECK(light.data == NULL);
    CHECK(psx_lgt_parse(file, 8 + NODES * TRACK_NODE_LIGHT_POINTS, NODES + 1, &light) == 0);   /* and the rows do not reach */
}

/* a tunnel row (a lit wall foot, the points beyond it 0) is read as the wall's half light, a bridge's dark terrain stays dark */
static void test_tunnel_wall_floor(void)
{
    static uint8_t rows[4 * TRACK_NODE_LIGHT_POINTS] = {
        255, 255, 126, 0, 0, 0, 255, 150, 0, 0, 0,   /* tunnel */
        255, 255, 10, 0, 0, 0, 255, 0, 0, 0, 0,      /* under a bridge: nothing lit */
        255, 255, 255, 0, 200, 0, 255, 40, 0, 0, 0,  /* lit wall foot, one point 0 beyond; no tunnel on either side */
        0,                                           /* the file's last node, not light */
    };
    const TrackNodeLight light = { .data = rows, .nodes = 4 };
    CHECK_EQ_INT(track_node_light_at(&light, 0, 2), 126);
    CHECK_EQ_INT(track_node_light_at(&light, 0, 3), TRACK_NODE_LIGHT_WALL);
    CHECK_EQ_INT(track_node_light_at(&light, 0, 5), TRACK_NODE_LIGHT_WALL);
    CHECK_EQ_INT(track_node_light_at(&light, 0, 8), TRACK_NODE_LIGHT_WALL);
    CHECK_EQ_INT(track_node_light_at(&light, 0, 0), TRACK_NODE_LIGHT_TUNNEL_ROAD);   /* the road of a tunnel row is dimmed (test_tunnel_road) */
    CHECK_EQ_INT(track_node_light_at(&light, 1, 3), 0);
    CHECK_EQ_INT(track_node_light_at(&light, 1, 9), 0);
    CHECK_EQ_INT(track_node_light_at(&light, 2, 3), 0);   /* a 0 beside a lit foot outside a tunnel (CY2's open road) is the black it says */
    CHECK_EQ_INT(track_node_light_at(&light, 2, 4), 200);
}

/* with the track's light script the points are the exe's: the context's colour in a tunnel, the LGT through T elsewhere, as grey; the fitted rules are not used */
static void test_script(void)
{
    static uint8_t rows[6 * TRACK_NODE_LIGHT_POINTS] = {
        255, 255, 255, 0, 0, 0, 255, 150, 0, 0, 0,   /* nodes 0, 1: a block with a RAMP context (the tunnel) */
        255, 255, 255, 0, 0, 0, 255, 150, 0, 0, 0,
        255, 255, 255, 0, 0, 0, 255, 150, 0, 0, 0,   /* nodes 2, 3 the same bytes */
        255, 255, 255, 0, 0, 0, 255, 150, 0, 0, 0,
        255, 255, 255, 0, 200, 0, 255, 40, 0, 60, 0, /* node 4: block 1, no context */
        0,
    };
    TrackNodeLight light = { .data = rows, .nodes = 6 };
    PsxLgs lgs = { { 0 }, { 128, 128, 128 }, { 128, 128, 128 }, 1, { { 0 } } };
    lgs.contexts[0] = (PsxLgsContext){ 0, 0, PSX_LGS_RAMP, { 128, 128, 128 }, { 60, 60, 60 } };
    CHECK(track_node_light_set_script(&light, &lgs));
    CHECK(track_node_light_tunnel(&light, 0) && track_node_light_tunnel(&light, 3));        /* the RAMP context's block, whatever the rows say */
    CHECK(!track_node_light_tunnel(&light, 4));                                      /* and not the rest, whatever the rows say */
    CHECK_EQ_INT(track_node_light_at(&light, 0, 0), 153);                            /* even node: colour 128 -> T 96 -> 0.6 */
    CHECK_EQ_INT(track_node_light_at(&light, 0, 8), 153);                            /* every point of the row the same: the LGT is not read */
    CHECK_EQ_INT(track_node_light_at(&light, 1, 0), 99);                             /* odd node: colour 60 -> T 62 -> 0.3875 */
    CHECK_EQ_INT(track_node_light_at(&light, 4, 0), 255);                            /* outside: the lit road is 1 */
    CHECK_EQ_INT(track_node_light_at(&light, 4, 3), 51);                             /* LGT 0 is 0.2, not black */
    CHECK_EQ_INT(track_node_light_at(&light, 4, 4), 210);                            /* LGT 200: T 32 + 100 = 132, 0.825 */
    CHECK_EQ_INT(track_node_light_byte(&light, 4, 4), 200);
    CHECK_EQ_INT(track_node_light_byte(&light, 5, 4), 200);                          /* the last node is not light: the one before */
    CHECK_EQ_INT(track_node_light_byte(&light, 4 + 6, 4), 200);                      /* the ring wraps */
    CHECK_EQ_INT(track_node_light_byte(NULL, 1, 1), TRACK_NODE_LIGHT_FULL);
    CHECK(!track_node_light_set_script(&light, NULL));
}

/* inside a tunnel (a lit wall foot with 0s beyond it, on either side) the road's own points are dimmed to TRACK_NODE_LIGHT_TUNNEL_ROAD: the console version
 * shows the road dark there although the file says 255; a darker byte stays, the open road and a bridge's rows are untouched */
static void test_tunnel_road(void)
{
    static uint8_t rows[5 * TRACK_NODE_LIGHT_POINTS] = {
        255, 255, 255, 0, 0, 0, 255, 150, 0, 0, 0,        /* tunnel, both sides */
        255, 255, 255, 0, 228, 228, 255, 255, 0, 218, 218, /* only a degenerate 0 at point 3 (CY2's open road): open */
        255, 255, 10, 0, 0, 0, 255, 0, 0, 0, 0,          /* under a bridge: foot dark */
        100, 100, 126, 0, 0, 0, 100, 128, 0, 0, 0,       /* a tunnel with a road darker than the dim already */
        0,
    };
    const TrackNodeLight light = { .data = rows, .nodes = 5 };
    CHECK_EQ_INT(track_node_light_at(&light, 0, 0), TRACK_NODE_LIGHT_TUNNEL_ROAD);
    CHECK_EQ_INT(track_node_light_at(&light, 0, 6), TRACK_NODE_LIGHT_TUNNEL_ROAD);
    CHECK_EQ_INT(track_node_light_at(&light, 0, 1), TRACK_NODE_LIGHT_TUNNEL_ROAD);
    CHECK_EQ_INT(track_node_light_at(&light, 0, 2), TRACK_NODE_LIGHT_WALL);            /* the wall's foot (the shoulder's far edge) is capped at half light... */
    CHECK_EQ_INT(track_node_light_at(&light, 0, 7), TRACK_NODE_LIGHT_WALL);            /* ...on both sides (150 -> 128) */
    CHECK_EQ_INT(track_node_light_at(&light, 3, 2), 126);                       /* a foot under the cap stays */
    CHECK_EQ_INT(track_node_light_at(&light, 1, 2), 255);                       /* and an open road's foot is its own */
    CHECK(track_node_light_tunnel(&light, 0) && track_node_light_tunnel(&light, 3));   /* the rows that are a tunnel's */
    CHECK(!track_node_light_tunnel(&light, 1) && !track_node_light_tunnel(&light, 2));
    CHECK(!track_node_light_tunnel(NULL, 0));
    CHECK_EQ_INT(track_node_light_at(&light, 1, 0), 255);
    CHECK_EQ_INT(track_node_light_at(&light, 1, 1), 255);
    CHECK_EQ_INT(track_node_light_at(&light, 2, 1), 255);
    CHECK_EQ_INT(track_node_light_at(&light, 3, 0), 100);
    CHECK_EQ_INT(track_node_light_at(&light, 3, 1), 100);
}

/* a track run the other way round (tri_reverse.h): the light is the same places' light, asked at the new node (j is the file's (N - j) mod N) and
 * the new side (the two chains of a row swap), with and without the script, in a tunnel and out */
static void test_reversed(void)
{
    enum { N = 12 };
    uint8_t rows[N * TRACK_NODE_LIGHT_POINTS];
    for (int n = 0; n < N; n++)
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) rows[n * TRACK_NODE_LIGHT_POINTS + p] = (uint8_t)(7 * n + 11 * p + 3);
    for (int n = 4; n < 8; n++) {   /* a tunnel: a lit left wall foot with 0s beyond it, on the file's left side only */
        uint8_t *row = rows + n * TRACK_NODE_LIGHT_POINTS;
        row[7] = 200, row[9] = 0, row[10] = 0;
    }
    PsxLgs lgs = { { 0 }, { 128, 128, 128 }, { 100, 100, 100 }, 1, { { 0 } } };
    lgs.contexts[0] = (PsxLgsContext){ 1, 1, PSX_LGS_RAMP, { 128, 128, 128 }, { 60, 60, 60 } };   /* block 1: nodes 4..7 */
    for (int scripted = 0; scripted < 2; scripted++) {
        TrackNodeLight forward = { .data = rows, .nodes = N }, back = { .data = rows, .nodes = N };
        if (scripted) CHECK(track_node_light_set_script(&forward, &lgs) && track_node_light_set_script(&back, &lgs));
        CHECK(track_node_light_set_reversed(&back, 1));
        for (uint32_t j = 0; j < N; j++) {
            const uint32_t old = (N - j) % N;
            CHECK_EQ_INT(track_node_light_old_node(&back, j), old);
            CHECK_EQ_INT(track_node_light_old_node(&back, j + N), old);   /* the ring wraps */
            CHECK_EQ_INT(track_node_light_tunnel(&back, j), track_node_light_tunnel(&forward, old));
            CHECK_EQ_INT(track_node_light_node(&back, j), track_node_light_node(&forward, old));
            for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) {
                const int side = p == 0 ? 0 : (p + 4) % 10 + 1;
                CHECK_EQ_INT(track_node_light_at(&back, j, p), track_node_light_at(&forward, old, side));
                CHECK_EQ_INT(track_node_light_byte(&back, j, p), track_node_light_byte(&forward, old, side));
            }
        }
        CHECK_EQ_INT(track_node_light_at(&back, 0, 7), track_node_light_at(&forward, 0, 2));   /* node 0 stays, its sides swap */
    }
    TrackNodeLight none = { 0 };
    CHECK(!track_node_light_set_reversed(&none, 1));
    CHECK_EQ_INT(track_node_light_old_node(NULL, 5), 5);
}

/* port (shoulder_light): the shoulder's far edge (points 2 and 7) is as lit as the road beside it before the beam scales it, so a car's beam lights it */
static void test_shoulder_light(void)
{
    uint8_t rows[3 * TRACK_NODE_LIGHT_POINTS];
    memset(rows, 20, sizeof rows);
    for (int n = 0; n < 3; n++) rows[n * TRACK_NODE_LIGHT_POINTS + 0] = rows[n * TRACK_NODE_LIGHT_POINTS + 1] = rows[n * TRACK_NODE_LIGHT_POINTS + 6] = 95;   /* the road */
    HeadlightPool pool;
    memset(&pool, 0, sizeof pool);
    pool.cone = 1, pool.count = 1;
    pool.cone_x[0] = 0.0, pool.cone_z[0] = 0.0, pool.cone_hx[0] = 0.0, pool.cone_hz[0] = 1.0;   /* a car at the origin facing +z */
    TrackNodeLight off = { .data = rows, .nodes = 3, .pool = &pool };
    TrackNodeLight on = off;
    on.shoulder_road = 1;
    const double at[2] = { 0.0, 8.0 };   /* in the beam */
    const int road = track_node_light_at_xz(&on, 1, 1, at), edge_off = track_node_light_at_xz(&off, 1, 7, at), edge_on = track_node_light_at_xz(&on, 1, 7, at);
    CHECK(edge_off < road / 2);                                        /* as it was: near black next to the lit road */
    CHECK(edge_on >= road - 2 && edge_on <= road + 2);                 /* the road's light, the beam on it alike */
    CHECK_EQ_INT(track_node_light_at_xz(&on, 1, 2, at), track_node_light_at_xz(&on, 1, 1, at));   /* the right side's point 2 too (points 1 and 2 share the road's) */
    CHECK_EQ_INT(track_node_light_at_xz(&on, 1, 3, at), track_node_light_at_xz(&off, 1, 3, at));  /* the terrain beyond is its own */
    CHECK_EQ_INT(track_node_light_at_xz(&on, 1, 0, at), track_node_light_at_xz(&off, 1, 0, at));  /* the road is untouched */
    const double far_at[2] = { 0.0, 150.0 };   /* beyond the beam's reach */
    CHECK_EQ_INT(track_node_light_at_xz(&on, 1, 7, far_at), track_node_light_at_xz(&off, 1, 7, far_at));   /* where there is no beam the row is as it was */
    TrackNodeLight dark = on;
    dark.pool = NULL;
    CHECK_EQ_INT(track_node_light_at_xz(&dark, 1, 7, at), 20);                /* by day (no pool) the row as it is */
}

/* port (pool_brightness): the headlights' pool does not depend on the file's `brightness` lines: rows halved by a retouch (data) over the rows before it (data_plain) light the pool as the plain
 * rows do, where the beam is full; the dark around it (no beam) still takes the retouch; no retouch (no data_plain) changes nothing */
static void test_pool_ignores_brightness(void)
{
    uint8_t plain[3 * TRACK_NODE_LIGHT_POINTS], half[3 * TRACK_NODE_LIGHT_POINTS];
    memset(plain, 200, sizeof plain);
    for (size_t i = 0; i < sizeof half; i++) half[i] = (uint8_t)(plain[i] / 2);   /* `brightness .. 50` */
    HeadlightPool pool;
    memset(&pool, 0, sizeof pool);
    pool.cone = 1, pool.count = 1;
    pool.cone_x[0] = 0.0, pool.cone_z[0] = 0.0, pool.cone_hx[0] = 0.0, pool.cone_hz[0] = 1.0;
    const TrackNodeLight reference = { .data = plain, .nodes = 3, .pool = &pool };
    const TrackNodeLight retouched = { .data = half, .data_plain = plain, .nodes = 3, .pool = &pool };
    const TrackNodeLight no_plain = { .data = half, .nodes = 3, .pool = &pool };
    const double in_beam[2] = { 0.0, 8.0 }, outside[2] = { 0.0, 150.0 };
    const int want = track_node_light_at_xz(&reference, 1, 1, in_beam);
    CHECK(want > 60);   /* a visible pool */
    CHECK_EQ_INT(track_node_light_at_xz(&retouched, 1, 1, in_beam), want);   /* brightness 50: the same pool as at 100 */
    CHECK(track_node_light_at_xz(&no_plain, 1, 1, in_beam) < want - 20);     /* (without the plain rows it dimmed: as before this change) */
    CHECK_EQ_INT(track_node_light_at_xz(&retouched, 1, 1, outside), track_node_light_at_xz(&no_plain, 1, 1, outside));   /* beyond the beam the retouch dims the scene as ever */
    CHECK(track_node_light_at_xz(&retouched, 1, 1, outside) < track_node_light_at_xz(&reference, 1, 1, outside));
    TrackNodeLight no_retouch = reference;   /* no brightness line: no plain copy, the same as ever */
    CHECK_EQ_INT(track_node_light_at_xz(&no_retouch, 1, 1, in_beam), want);
}

/* port (tunnel_pool): at night (with the pool) a tunnel whose lamps are at 0 % (lamps_off) is lit by the car's beam as the open ground is and is drawn as it (class 0); a lit tunnel keeps its own
 * way, and by day (no pool) nothing changes */
static void test_unlit_tunnel_has_the_pool(void)
{
    uint8_t rows[3 * TRACK_NODE_LIGHT_POINTS];
    memset(rows, 0, sizeof rows);
    for (int n = 0; n < 3; n++) rows[n * TRACK_NODE_LIGHT_POINTS + 0] = rows[n * TRACK_NODE_LIGHT_POINTS + 1] = rows[n * TRACK_NODE_LIGHT_POINTS + 6] = 255;   /* the road */
    HeadlightPool pool;
    memset(&pool, 0, sizeof pool);
    pool.cone = 1, pool.count = 1;
    pool.cone_x[0] = 0.0, pool.cone_z[0] = 0.0, pool.cone_hx[0] = 0.0, pool.cone_hz[0] = 1.0;
    uint8_t tunnel[3] = { 1, 1, 1 }, unlit_marks[3] = { 1, 1, 1 };
    const TrackNodeLight open = { .data = rows, .nodes = 3, .pool = &pool };
    const TrackNodeLight lit_tunnel = { .data = rows, .nodes = 3, .pool = &pool, .tunnel = tunnel };
    const TrackNodeLight unlit = { .data = rows, .nodes = 3, .pool = &pool, .tunnel = tunnel, .lamps_off = unlit_marks };
    TrackNodeLight by_day = unlit;
    by_day.pool = NULL;
    const double in_beam[2] = { 0.0, 8.0 };
    const int want = track_node_light_at_xz(&open, 1, 1, in_beam);
    CHECK(want > 150);   /* the open road's pool */
    CHECK_EQ_INT(track_node_light_at_xz(&unlit, 1, 1, in_beam), want);   /* an unlit tunnel's: the same */
    CHECK_EQ_INT(track_node_light_vertex_class(&unlit, 1, 1), TRACK_NODE_LIGHT_CLASS_UNLIT);   /* its own tables: the look's colour, the beam in the light */
    CHECK_EQ_INT(track_node_light_vertex_class(&lit_tunnel, 1, 1), 1);    /* a lit tunnel is still a tunnel */
    CHECK(track_node_light_at_xz(&lit_tunnel, 1, 1, in_beam) != want);   /* and keeps its own light */
    CHECK_EQ_INT(track_node_light_vertex_class(&by_day, 1, 1), 1);        /* by day (no pool) the tunnel is as it was */
    CHECK_EQ_INT(track_node_light_at_xz(&by_day, 1, 1, in_beam), TRACK_NODE_LIGHT_TUNNEL_ROAD);
}

/* port (tunnel_pool): by day, dawn and dusk (no night pool) the player's beam lights only a tunnel whose lamps are at 0 %, from the tunnel's own look as the dark; every other node, the open road above
 * all, gets exactly its row: a pool that escaped to the open road by day would show here */
static void test_day_tunnel_pool_stays_in_the_tunnel(void)
{
    enum { N = 6 };
    uint8_t rows[N * TRACK_NODE_LIGHT_POINTS], tunnel[N] = { 0, 0, 1, 1, 0, 0 }, lamps_off[N] = { 0, 0, 1, 1, 0, 0 };
    for (int n = 0; n < N; n++)
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) rows[n * TRACK_NODE_LIGHT_POINTS + p] = (uint8_t)(p == 0 || p == 1 || p == 6 ? 200 : 30 + 7 * p);
    HeadlightPool pool;
    memset(&pool, 0, sizeof pool);
    pool.cone = 1, pool.count = 1;
    pool.cone_x[0] = 0.0, pool.cone_z[0] = 0.0, pool.cone_hx[0] = 0.0, pool.cone_hz[0] = 1.0;
    TrackNodeLight light = { .data = rows, .nodes = N, .tunnel = tunnel, .lamps_off = lamps_off, .tunnel_pool = &pool, .look_set = 1, .look_light = 40.0 };
    const double in_beam[2] = { 0.0, 8.0 }, outside[2] = { 0.0, 150.0 };
    for (int n = 0; n < N; n++)
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++)
            if (!tunnel[n]) CHECK_EQ_INT(track_node_light_at_xz(&light, (uint32_t)n, p, in_beam), rows[n * TRACK_NODE_LIGHT_POINTS + p]);   /* open road: the row, beam or not */
    const int lit = track_node_light_at_xz(&light, 2, 1, in_beam), dark = track_node_light_at_xz(&light, 2, 1, outside);
    CHECK(dark >= 200 * 40 / 100 - 3 && dark <= 200 * 40 / 100 + 3);   /* the tunnel's own look (40 %) is its dark */
    CHECK(lit > dark + 40);                                              /* and the beam lifts it */
    pool.count = 0;   /* no lamps lit: the tunnel is as dark as its look, nothing else changes */
    CHECK_EQ_INT(track_node_light_at_xz(&light, 2, 1, in_beam), dark);
    TrackNodeLight no_pool = light;
    no_pool.tunnel_pool = NULL;
    CHECK_EQ_INT(track_node_light_at_xz(&no_pool, 2, 1, in_beam), TRACK_NODE_LIGHT_TUNNEL_ROAD);   /* with no pool at all (Stock, Restored): as ever */
}

/* port (`lamp`): which lamps reach a polygon (by node, the ring's seam included), the light of the lamps at a ground point (strength, falloff, colour) and the car's light by its distance to the lamp */
static void test_radial_lamps(void)
{
    TrackNodeLightLamp lamps[3] = { { 10, 0.0f, 0.0f, 30.0f, { 1.0f, 0.9f, 0.55f }, 0, { 0, 0 }, 0, 0 }, { 14, 24.0f, 0.0f, 30.0f, { 1.0f, 0.0f, 0.0f }, 0, { 0, 0 }, 0, 0 }, { 100, 600.0f, 0.0f, 30.0f, { 1, 1, 1 }, 0, { 0, 0 }, 0, 0 } };
    TrackNodeLight l = { 0 };
    l.nodes = 128, l.lamps = lamps, l.lamp_n = 3, l.lamp_reach_nodes = 5, l.lamp_placed = 1;
    int idx[8];
    const uint32_t at12[4] = { 12, 12, 13, 13 };
    CHECK_EQ_INT(track_node_light_lamps_near(&l, at12, idx, 8), 2);   /* nodes 10 and 14, not 100 */
    CHECK(idx[0] == 0 && idx[1] == 1);
    const uint32_t none[4] = { 50, 50, 51, 51 };
    CHECK_EQ_INT(track_node_light_lamps_near(&l, none, idx, 8), 0);
    const uint32_t nowhere[4] = { TRACK_NODE_LIGHT_NO_NODE, 0, 0, 0 };
    CHECK_EQ_INT(track_node_light_lamps_near(&l, nowhere, idx, 8), 0);
    TrackNodeLightLamp edge[1] = { { 2, 0.0f, 0.0f, 30.0f, { 1, 1, 1 }, 0, { 0, 0 }, 0, 0 } };   /* a ring: the lamp at 2 is 5 nodes from node 125 across the seam */
    TrackNodeLight r = { 0 };
    r.nodes = 128, r.lamps = edge, r.lamp_n = 1, r.lamp_reach_nodes = 5, r.lamp_ring = 1;
    const uint32_t seam[4] = { 125, 125, 126, 126 };
    CHECK_EQ_INT(track_node_light_lamps_near(&r, seam, idx, 8), 1);
    r.lamp_ring = 0;
    CHECK_EQ_INT(track_node_light_lamps_near(&r, seam, idx, 8), 0);   /* a stage has no seam */
    float rgb[3];
    CHECK(track_node_light_lamp_at(&l, idx, 0, 0.0, 0.0, rgb) == 0.0f && rgb[0] == 0.0f && rgb[2] == 0.0f);   /* no lamp: no colour, not garbage */
    const int one[1] = { 0 };
    CHECK(track_node_light_lamp_at(&l, one, 1, 0.0, 0.0, rgb) == 1.0f && rgb[0] == 1.0f && rgb[1] == 0.9f);   /* under the lamp: full, its colour */
    const float half = track_node_light_lamp_at(&l, one, 1, 15.0, 0.0, rgb);
    CHECK(half == 0.25f && track_node_light_lamp_at(&l, one, 1, 30.0, 0.0, rgb) == 0.0f);   /* smooth to nothing at the reach */
    const int two[2] = { 0, 1 };
    const float mix = track_node_light_lamp_at(&l, two, 2, 12.0, 0.0, rgb);   /* between a warm and a red lamp: their colours' mean, weighted by what each gives */
    CHECK(mix > 0.0f && rgb[0] == 1.0f && rgb[1] > 0.0f && rgb[1] < 0.9f);
    /* the car: the lamp's colour times LAMP_CAR_ADD times its light is added, nothing darkens, none beyond the reach */
    float out[3] = { 0.5f, 0.5f, 0.5f };
    CHECK(track_node_light_lamp_car(&l, 10, 0.0, 0.0, out));
    CHECK(out[0] > 0.5f + LAMP_CAR_ADD - 0.01f && out[1] > 0.5f && out[2] > 0.5f && out[2] < out[1] && out[1] < out[0]);
    float away[3] = { 0.5f, 0.5f, 0.5f };
    CHECK(!track_node_light_lamp_car(&l, 10, -40.0, 0.0, away) && away[0] == 0.5f);
    float near[3] = { 0.5f, 0.5f, 0.5f };
    CHECK(track_node_light_lamp_car(&l, 10, 15.0, 0.0, near) && near[0] > 0.5f && near[0] < out[0]);   /* less away from it */
    float bright[3] = { 1.9f, 1.9f, 1.9f };
    track_node_light_lamp_car(&l, 10, 0.0, 0.0, bright);
    CHECK(bright[0] <= 2.0f);   /* the old pool's ceiling */
    /* a tunnel lamp lifts the car to full light at most (the between-lamps level up to the lit one), not to the street lamp's ceiling; a car already above it is left */
    TrackNodeLightLamp tl[1] = { { 10, 0.0f, 0.0f, 30.0f, { 5.0f, 4.0f, 2.5f }, 1, { 0, 0 }, 0, 0 } };
    TrackNodeLight t = { 0 };
    t.nodes = 128, t.lamps = tl, t.lamp_n = 1, t.lamp_reach_nodes = 5, t.lamp_placed = 1;
    float under[3] = { 0.75f, 0.75f, 0.75f };
    CHECK(track_node_light_lamp_car(&t, 10, 0.0, 0.0, under) && under[0] == 1.0f && under[1] <= 1.0f && under[2] <= 1.0f);
    float side[3] = { 0.75f, 0.75f, 0.75f };
    CHECK(track_node_light_lamp_car(&t, 10, 15.0, 0.0, side) && side[0] > 0.75f && side[0] < 1.0f + 1e-6f);
    float over[3] = { 1.5f, 1.5f, 1.5f };
    track_node_light_lamp_car(&t, 10, 0.0, 0.0, over);
    CHECK(over[0] == 1.5f);
}

/* a lamp with a reach of its own along the road (a tunnel's: `reach_along`, `along` the road's direction) is an ellipse: short along the road, its reach across */
static void test_lamp_shaped_along_the_road(void)
{
    TrackNodeLightLamp lamps[1] = { { 10, 0.0f, 0.0f, 30.0f, { 1, 1, 1 }, 1, { 0.0f, 1.0f }, 10.0f, 0 } };
    TrackNodeLight l = { 0 };
    l.nodes = 128, l.lamps = lamps, l.lamp_n = 1, l.lamp_reach_nodes = 5, l.lamp_placed = 1;
    const int one[1] = { 0 };
    float rgb[3];
    CHECK(track_node_light_lamp_at(&l, one, 1, 0.0, 0.0, rgb) == 1.0f);
    CHECK(fabsf(track_node_light_lamp_at(&l, one, 1, 0.0, 5.0, rgb) - 0.25f) < 1e-5f);   /* half its reach along the road */
    CHECK(track_node_light_lamp_at(&l, one, 1, 0.0, 10.0, rgb) == 0.0f);                  /* none at the reach along: the gap to the next lamp */
    CHECK(track_node_light_lamp_at(&l, one, 1, 0.0, -10.0, rgb) == 0.0f);
    CHECK(fabsf(track_node_light_lamp_at(&l, one, 1, 15.0, 0.0, rgb) - 0.25f) < 1e-5f);   /* across: the whole reach, the walls */
    const float diagonal = 1.0f - sqrtf(2.0f) * 7.5f / 30.0f;   /* a quarter of each reach: 7.5 * sqrt(2) of 30, an ellipse, not a square */
    CHECK(fabsf(track_node_light_lamp_at(&l, one, 1, 7.5, 2.5, rgb) - diagonal * diagonal) < 1e-5f);
    lamps[0].reach_along = 0.0f;   /* none of its own: round, as the street lamps */
    CHECK(fabsf(track_node_light_lamp_at(&l, one, 1, 0.0, 15.0, rgb) - 0.25f) < 1e-5f);
}

/* an object beside a lamp: raised to what the lamp gives at its own place, less what the rows' bake already put on its terrain's points (not added twice) */
static void test_object_lamp(void)
{
    TrackNodeLightLamp lamp[1] = { { 10, 0.0f, 0.0f, 30.0f, { 1.0f, 0.9f, 0.55f }, 0, { 0, 0 }, 0, 0 } };
    static uint8_t lift[128 * TRACK_NODE_LIGHT_POINTS];
    TrackNodeLight l = { 0 };
    l.nodes = 128, l.lamps = lamp, l.lamp_n = 1, l.lamp_reach_nodes = 5, l.lamp_placed = 1;
    CHECK_EQ_INT(track_node_light_object_lamp(&l, 10, 0.0, 0.0, 100), 100);   /* no bake (lamp_lift NULL): the rows are as they were */
    l.lamp_lift = lift;
    CHECK_EQ_INT(track_node_light_object_lamp(&l, 10, 0.0, 0.0, 100), 255);   /* under the lamp, nothing baked there: full */
    const int half = track_node_light_object_lamp(&l, 10, 15.0, 0.0, 100);    /* a quarter of the lamp at 15 m: a quarter of the way to full */
    CHECK(half > 100 && half < 255 && half == 100 + (int)(155 * 0.25f + 0.5f));
    CHECK_EQ_INT(track_node_light_object_lamp(&l, 10, -40.0, 0.0, 100), 100);   /* beyond the reach */
    CHECK_EQ_INT(track_node_light_object_lamp(&l, 60, 0.0, 0.0, 100), 100);     /* a node no lamp is near */
    CHECK_EQ_INT(track_node_light_object_lamp(&l, TRACK_NODE_LIGHT_NO_NODE, 0.0, 0.0, 100), 100);
    for (int n = 0; n < 128; n++)   /* the terrain's points fully baked: the object has nothing more to take */
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) lift[n * TRACK_NODE_LIGHT_POINTS + p] = 255;
    CHECK_EQ_INT(track_node_light_object_lamp(&l, 10, 0.0, 0.0, 100), 100);
    for (int n = 0; n < 128; n++)   /* half baked: under the lamp it takes the rest of the way, at 15 m (a quarter) it is already past it */
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) lift[n * TRACK_NODE_LIGHT_POINTS + p] = 128;
    CHECK_EQ_INT(track_node_light_object_lamp(&l, 10, 0.0, 0.0, 180), 255);
    CHECK_EQ_INT(track_node_light_object_lamp(&l, 10, 15.0, 0.0, 180), 180);
    CHECK_EQ_INT(track_node_light_object_lamp(NULL, 10, 0.0, 0.0, 77), 77);
}

/* a made light's tunnel marks decide: the approach rows the synth ramps in (a lit foot, 0s beyond) are not the tunnel before its first marked node */
static void test_marks_start_the_tunnel(void)
{
    static uint8_t rows[5 * TRACK_NODE_LIGHT_POINTS] = {
        255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255,   /* open */
        255, 255, 99, 0, 0, 0, 255, 255, 255, 255, 255,    /* the mouth's ramp: a foot, 0s beyond, not marked */
        255, 255, 150, 0, 0, 0, 255, 255, 255, 255, 255,   /* ramp, not marked */
        255, 255, 192, 0, 0, 0, 255, 150, 0, 0, 0,         /* marked */
        0,
    };
    static uint8_t marks[5] = { 0, 0, 0, 1, 0 };
    const TrackNodeLight light = { .data = rows, .nodes = 5, .tunnel = marks };
    CHECK(!track_node_light_tunnel(&light, 1));
    CHECK(!track_node_light_tunnel(&light, 2));
    CHECK(track_node_light_tunnel(&light, 3));
}

/* an object standing at a tunnel's mouth (the portal) takes the OPEN road's light: the mean of its row's road points (0, 1, 2, 6, 7), not the terrain points the mountain's shadow and the tunnel's own
 * darkening reach; every other node, inside or away from a mouth, is lit as before */
static void test_a_mouth_object_takes_the_open_roads_light(void)
{
    static uint8_t rows[8 * TRACK_NODE_LIGHT_POINTS];
    for (int n = 0; n < 8; n++)
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) rows[n * TRACK_NODE_LIGHT_POINTS + p] = (p == 0 || p == 1 || p == 2 || p == 6 || p == 7) ? 200 : 20;   /* a bright road, shaded terrain */
    static uint8_t marks[8] = { 0, 0, 0, 1, 1, 0, 0, 0 };   /* a tunnel on nodes 3, 4: the mouths are 2 and 5 */
    const TrackNodeLight light = { .data = rows, .nodes = 8, .tunnel = marks };
    CHECK_EQ_INT(track_node_light_node(&light, 2), 65);   /* the terrain's mean: points 2 and 7 bright, 3, 4, 5, 8, 9, 10 dark: (200 + 200 + 20 * 6) / 8 */
    CHECK_EQ_INT(track_node_light_object_node(&light, 2, NULL), 200);   /* the entrance's mouth node: the road's */
    CHECK_EQ_INT(track_node_light_object_node(&light, 5, NULL), 200);   /* the exit's */
    CHECK_EQ_INT(track_node_light_object_node(&light, 3, NULL), track_node_light_node(&light, 3));   /* inside: as before */
    CHECK_EQ_INT(track_node_light_object_node(&light, 1, NULL), track_node_light_node(&light, 1));   /* away from the mouth: as before */
    CHECK_EQ_INT(track_node_light_object_node(&light, 7, NULL), track_node_light_node(&light, 7));
    CHECK_EQ_INT(track_node_light_object_node(NULL, 2, NULL), TRACK_NODE_LIGHT_FULL);
    const TrackNodeLight none = { .data = rows, .nodes = 8 };   /* no tunnel marks: nothing is a mouth */
    CHECK_EQ_INT(track_node_light_object_node(&none, 2, NULL), track_node_light_node(&none, 2));
}

/* a tunnel mouth's gradient on the road's light: the road inside a tunnel is clamped to TRACK_NODE_LIGHT_TUNNEL_ROAD (the shoulder to TRACK_NODE_LIGHT_WALL), and over the gradient's steps that clamp comes in
 * by the step's blend squared (25, 50, 75 % come in as 6, 25, 56 %: the darkening is steep at the bright end, so a straight share was a cliff at the first step) from the row's own bytes, node by node, at both mouths; with no steps the clamp is whole from the first node, as before */
static void test_the_gradient_grades_the_tunnels_road_clamp(void)
{
    enum { N = 12 };
    static uint8_t rows[N * TRACK_NODE_LIGHT_POINTS];
    for (int n = 0; n < N; n++)
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) rows[n * TRACK_NODE_LIGHT_POINTS + p] = (p == 0 || p == 1 || p == 6) ? 215 : p == 2 || p == 7 ? 196 : 0;
    static uint8_t marks[N] = { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0 };   /* a tunnel on nodes 2..9 */
    static uint8_t ids[N] = { 0, 0, 1, 2, 3, 0, 0, 3, 2, 1, 0, 0 };    /* its steps: 25, 50, 75 % at each mouth */
    TrackNodeLightTone tones[TRACK_NODE_LIGHT_TONES];
    memset(tones, 0, sizeof tones);
    for (int i = 0; i < 3; i++) tones[i].look = 2, tones[i].blend = (uint8_t)(25 * (i + 1));
    static uint16_t mask[N];
    for (int n = 0; n < N; n++) mask[n] = 0x7ff;
    TrackNodeLight light = { .data = rows, .nodes = N, .tunnel = marks, .tone_id = ids, .tone_mask = mask, .tone_count = 3 };
    memcpy(light.tone, tones, sizeof tones);
    const int road[] = { track_node_light_at(&light, 2, 0), track_node_light_at(&light, 3, 0), track_node_light_at(&light, 4, 0), track_node_light_at(&light, 5, 0) };
    CHECK_EQ_INT(road[0], 215 - (215 - TRACK_NODE_LIGHT_TUNNEL_ROAD) * 37 / 100);   /* the mouth's first node: the step's blend and a half of it of the way in (25 % comes in as 37 %) */
    CHECK_EQ_INT(road[1], 215 - (215 - TRACK_NODE_LIGHT_TUNNEL_ROAD) * 75 / 100);
    CHECK_EQ_INT(road[2], TRACK_NODE_LIGHT_TUNNEL_ROAD);                              /* 75 % comes in whole: the tunnel's road two nodes in */
    CHECK_EQ_INT(road[3], TRACK_NODE_LIGHT_TUNNEL_ROAD);                              /* past the steps: the clamp, whole */
    CHECK(road[0] > road[1] && road[1] > road[2] && road[2] >= road[3]);
    for (int k = 0; k < 4; k++) CHECK_EQ_INT(track_node_light_at(&light, 9 - (uint32_t)k, 1), road[k]);   /* the other mouth: the same, mirrored; point 1 as point 0 */
    CHECK_EQ_INT(track_node_light_at(&light, 2, 2), 196 - (196 - TRACK_NODE_LIGHT_WALL) * 37 / 100);   /* the shoulder's far edge too */
    CHECK_EQ_INT(track_node_light_at(&light, 5, 2), TRACK_NODE_LIGHT_WALL);
    CHECK_EQ_INT(track_node_light_at(&light, 1, 0), 215);                              /* outside: the row's bytes */
    light.tone_id = NULL;                                                              /* no steps: the clamp whole from the first node, as before */
    CHECK_EQ_INT(track_node_light_at(&light, 2, 0), TRACK_NODE_LIGHT_TUNNEL_ROAD);
}

/* the open road before a tunnel's mouth (and past its other end) darkens over the gradient's outer steps (look 3, `share` the percent of the way from the row's byte to TRACK_NODE_LIGHT_MOUTH_OPEN), so it
 * meets the tunnel's own dark road surface: node by node, at both ends, whatever the row's byte; the nodes past the steps and the tunnel's own are as before */
static void test_the_gradient_darkens_the_open_road_before_the_mouth(void)
{
    enum { N = 16 };
    static uint8_t rows[N * TRACK_NODE_LIGHT_POINTS];
    for (int n = 0; n < N; n++)
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) rows[n * TRACK_NODE_LIGHT_POINTS + p] = (p == 0 || p == 1 || p == 6) ? 255 : p == 2 || p == 7 ? 255 : 0;
    static uint8_t marks[N] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };   /* a tunnel on nodes 5..10 */
    static uint8_t ids[N] = { 0, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 3, 2, 1, 0, 0 };    /* its outer steps: 25, 50, 75 % of the way at each end, the shallowest first */
    TrackNodeLightTone tones[TRACK_NODE_LIGHT_TONES];
    memset(tones, 0, sizeof tones);
    for (int i = 0; i < 3; i++) tones[i].look = 3, tones[i].blend = (uint8_t)(8 * (i + 1)), tones[i].share = (uint8_t)(25 * (i + 1));
    static uint16_t mask[N];
    for (int n = 0; n < N; n++) mask[n] = 0x7ff;
    TrackNodeLight light = { .data = rows, .nodes = N, .tunnel = marks, .tone_id = ids, .tone_mask = mask, .tone_count = 3 };
    memcpy(light.tone, tones, sizeof tones);
    int road[3];
    for (int k = 0; k < 3; k++) road[k] = track_node_light_at(&light, (uint32_t)(2 + k), 0);
    for (int k = 0; k < 3; k++) CHECK_EQ_INT(road[k], 255 - (255 - TRACK_NODE_LIGHT_MOUTH_OPEN) * 25 * (k + 1) / 100);
    CHECK(road[0] > road[1] && road[1] > road[2] && road[2] > TRACK_NODE_LIGHT_MOUTH_OPEN);
    for (int k = 0; k < 3; k++) CHECK_EQ_INT(track_node_light_at(&light, (uint32_t)(13 - k), 1), road[k]);   /* the other end: the same, mirrored; point 1 as point 0 */
    CHECK_EQ_INT(track_node_light_at(&light, 1, 0), 255);                                                     /* past the steps: the row's byte */
    CHECK_EQ_INT(track_node_light_at(&light, 5, 0), TRACK_NODE_LIGHT_TUNNEL_ROAD);                           /* the tunnel's own: the clamp */
    CHECK_EQ_INT(track_node_light_at(&light, 4, 2), 255 - (255 - TRACK_NODE_LIGHT_WALL) * 75 / 100);         /* the shoulder's far edge goes toward the wall's light */
    CHECK_EQ_INT(track_node_light_at(&light, 4, 3), 0);                                                       /* a point with no road in it is left */
    TrackNodeLight back = light;   /* a reversed track (Mirror reverses nothing: the steps are by node): every node and point is the file's node it came from, steps and all */
    CHECK(track_node_light_set_reversed(&back, 1));
    for (uint32_t j = 0; j < N; j++)
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) CHECK_EQ_INT(track_node_light_at(&back, j, p), track_node_light_at(&light, track_node_light_old_node(&back, j), p == 0 ? 0 : (p + 4) % 10 + 1));
    light.tone_id = NULL;
    CHECK_EQ_INT(track_node_light_at(&light, 4, 0), 255);                                                     /* no steps: as before */
}

/* at night the outer ramp is there too (the open road before an unlit mouth is not a cliff), and the headlight pool lifts over it as it does on any open road: where the beam is the road is as it would be with
 * no steps; away from the beam the ramp darkens it as without a pool */
static void test_the_pool_lifts_over_the_outer_ramp(void)
{
    enum { N = 16 };
    static uint8_t rows[N * TRACK_NODE_LIGHT_POINTS];
    for (int n = 0; n < N; n++)
        for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++) rows[n * TRACK_NODE_LIGHT_POINTS + p] = (p == 0 || p == 1 || p == 6) ? 120 : 0;
    static uint8_t marks[N] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
    static uint8_t ids[N] = { 0, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 3, 2, 1, 0, 0 };
    static uint16_t mask[N];
    for (int n = 0; n < N; n++) mask[n] = 0x7ff;
    static HeadlightPool pool;
    pool.count = 1, pool.node[0] = 0, pool.ring = N;   /* a car at node 0: the light is full from 3 to 5 nodes ahead of it, over nodes 3 and 4 of the ramp */
    TrackNodeLight light = { .data = rows, .nodes = N, .tunnel = marks, .tone_id = ids, .tone_mask = mask, .tone_count = 3, .pool = &pool };
    for (int i = 0; i < 3; i++) light.tone[i].look = 3, light.tone[i].blend = (uint8_t)(8 * (i + 1)), light.tone[i].share = (uint8_t)(25 * (i + 1));
    uint8_t lit[3], flat[3];
    for (uint32_t k = 0; k < 3; k++) lit[k] = track_node_light_at(&light, 2 + k, 0);
    TrackNodeLight bare = light;
    bare.tone_id = NULL;   /* the same pool, no steps */
    for (uint32_t k = 0; k < 3; k++) flat[k] = track_node_light_at(&bare, 2 + k, 0);
    for (int k = 0; k < 3; k++) CHECK(lit[k] <= flat[k]);   /* never above the road as it would be without steps */
    CHECK_EQ_INT(lit[1], flat[1]);                          /* in the beam's full light the road is the whole road's: as if the file had no steps */
    CHECK_EQ_INT(lit[2], flat[2]);
    pool.node[0] = 8;   /* the car past the mouth: the nodes of the ramp are in the dark, and the ramp darkens them node by node as without a pool */
    uint8_t dim[3], dim_flat[3];
    for (uint32_t k = 0; k < 3; k++) dim[k] = track_node_light_at(&light, 2 + k, 0), dim_flat[k] = track_node_light_at(&bare, 2 + k, 0);
    CHECK(dim[0] < dim_flat[0] && dim[1] < dim[0] && dim[2] < dim[1]);
}

int main(void)
{
    test_a_mouth_object_takes_the_open_roads_light();
    test_the_gradient_darkens_the_open_road_before_the_mouth();
    test_the_pool_lifts_over_the_outer_ramp();
    test_the_gradient_grades_the_tunnels_road_clamp();
    test_marks_start_the_tunnel();
    test_tunnel_road();
    test_script();
    test_tunnel_wall_floor();
    test_parse_and_lookup();
    test_object_floor_lifts_only_the_points_beyond_the_road();
    test_rejected_files();
    test_reversed();
    test_shoulder_light();
    test_pool_ignores_brightness();
    test_unlit_tunnel_has_the_pool();
    test_day_tunnel_pool_stays_in_the_tunnel();
    test_radial_lamps();
    test_lamp_shaped_along_the_road();
    test_object_lamp();
    return TEST_REPORT();
}
