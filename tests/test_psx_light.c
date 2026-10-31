/* The PS1 look's baked light (psx_light.h) on made-up rows: the header's checks, the node and point lookups, the file's
 * garbage last node. No game or CD data. */
#include "game/race/psx_light.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { NODES = 6, TRAILER = 12 };

static size_t make_file(uint8_t *out, uint32_t nodes, uint32_t header_nodes)
{
    memset(out, 0, 8 + (size_t)nodes * PSX_LIGHT_POINTS + TRAILER);
    out[0] = (uint8_t)header_nodes, out[4] = 0;
    out[5] = 1;   /* scale 256 */
    for (uint32_t n = 0; n < nodes; n++)
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) out[8 + n * PSX_LIGHT_POINTS + (uint32_t)p] = (uint8_t)(16 * n + p);
    memcpy(out + 8 + (size_t)nodes * PSX_LIGHT_POINTS, "CRCF", 4);
    return 8 + (size_t)nodes * PSX_LIGHT_POINTS + TRAILER;
}

static void test_parse_and_lookup(void)
{
    uint8_t file[8 + NODES * PSX_LIGHT_POINTS + TRAILER];
    const size_t size = make_file(file, NODES, NODES);
    PsxLight light = { 0 };
    CHECK(psx_light_parse(file, size, NODES, &light));
    CHECK_EQ_INT(light.nodes, NODES);
    CHECK_EQ_INT(psx_light_at(&light, 0, 0), 0);
    CHECK_EQ_INT(psx_light_at(&light, 2, 7), 16 * 2 + 7);
    CHECK_EQ_INT(psx_light_at(&light, 2 + NODES, 7), 16 * 2 + 7);           /* the ring wraps */
    CHECK_EQ_INT(psx_light_at(&light, NODES - 1, 3), 16 * (NODES - 2) + 3);   /* the last node is not light */
    CHECK_EQ_INT(psx_light_at(&light, 1, -1), PSX_LIGHT_FULL);
    CHECK_EQ_INT(psx_light_at(&light, 1, PSX_LIGHT_POINTS), PSX_LIGHT_FULL);
    CHECK_EQ_INT(psx_light_at(NULL, 1, 1), PSX_LIGHT_FULL);
    CHECK_EQ_INT(psx_light_node(&light, 2), 38);   /* the terrain points' mean (32 + 2..5, 7..10), the road's left out */
    CHECK_EQ_INT(psx_light_node(NULL, 2), PSX_LIGHT_FULL);
    psx_light_free(&light);
    CHECK(light.data == NULL);
    CHECK_EQ_INT(psx_light_at(&light, 1, 1), PSX_LIGHT_FULL);
    CHECK_EQ_INT(psx_light_node(&light, 1), PSX_LIGHT_FULL);
}

static void test_rejected_files(void)
{
    uint8_t file[8 + NODES * PSX_LIGHT_POINTS + TRAILER];
    PsxLight light = { 0 };
    const size_t size = make_file(file, NODES, NODES);
    CHECK(!psx_light_parse(file, size, NODES + 1, &light));                     /* another track's node count */
    CHECK(!psx_light_parse(file, 8 + (NODES - 1) * PSX_LIGHT_POINTS, NODES, &light));   /* rows cut short */
    CHECK(!psx_light_parse(file, 4, NODES, &light));
    CHECK(!psx_light_parse(NULL, size, NODES, &light));
    make_file(file, NODES, NODES + 1);
    CHECK(!psx_light_parse(file, size, NODES, &light));                         /* the header says otherwise */
    CHECK(light.data == NULL);
    CHECK(psx_light_parse(file, 8 + NODES * PSX_LIGHT_POINTS, NODES + 1, &light) == 0);   /* and the rows do not reach */
}

/* a tunnel row (a lit wall foot, the points beyond it 0) is read as the wall's half light, a bridge's dark terrain stays dark */
static void test_tunnel_wall_floor(void)
{
    static uint8_t rows[4 * PSX_LIGHT_POINTS] = {
        255, 255, 126, 0, 0, 0, 255, 150, 0, 0, 0,   /* tunnel */
        255, 255, 10, 0, 0, 0, 255, 0, 0, 0, 0,      /* under a bridge: nothing lit */
        255, 255, 255, 0, 200, 0, 255, 40, 0, 0, 0,  /* lit wall foot, one point 0 beyond; no tunnel on either side */
        0,                                           /* the file's last node, not light */
    };
    const PsxLight light = { .data = rows, .nodes = 4 };
    CHECK_EQ_INT(psx_light_at(&light, 0, 2), 126);
    CHECK_EQ_INT(psx_light_at(&light, 0, 3), PSX_LIGHT_WALL);
    CHECK_EQ_INT(psx_light_at(&light, 0, 5), PSX_LIGHT_WALL);
    CHECK_EQ_INT(psx_light_at(&light, 0, 8), PSX_LIGHT_WALL);
    CHECK_EQ_INT(psx_light_at(&light, 0, 0), PSX_LIGHT_TUNNEL_ROAD);   /* the road of a tunnel row is dimmed (test_tunnel_road) */
    CHECK_EQ_INT(psx_light_at(&light, 1, 3), 0);
    CHECK_EQ_INT(psx_light_at(&light, 1, 9), 0);
    CHECK_EQ_INT(psx_light_at(&light, 2, 3), 0);   /* a 0 beside a lit foot outside a tunnel (CY2's open road) is the black it says */
    CHECK_EQ_INT(psx_light_at(&light, 2, 4), 200);
}

/* with the track's light script the points are the exe's: the context's colour in a tunnel, the LGT through T elsewhere, as grey; the fitted rules are not used */
static void test_script(void)
{
    static uint8_t rows[6 * PSX_LIGHT_POINTS] = {
        255, 255, 255, 0, 0, 0, 255, 150, 0, 0, 0,   /* nodes 0, 1: a block with a RAMP context (the tunnel) */
        255, 255, 255, 0, 0, 0, 255, 150, 0, 0, 0,
        255, 255, 255, 0, 0, 0, 255, 150, 0, 0, 0,   /* nodes 2, 3 the same bytes */
        255, 255, 255, 0, 0, 0, 255, 150, 0, 0, 0,
        255, 255, 255, 0, 200, 0, 255, 40, 0, 60, 0, /* node 4: block 1, no context */
        0,
    };
    PsxLight light = { .data = rows, .nodes = 6 };
    PsxLgs lgs = { { 0 }, { 128, 128, 128 }, { 128, 128, 128 }, 1, { { 0 } } };
    lgs.contexts[0] = (PsxLgsContext){ 0, 0, PSX_LGS_RAMP, { 128, 128, 128 }, { 60, 60, 60 } };
    CHECK(psx_light_set_script(&light, &lgs));
    CHECK(psx_light_tunnel(&light, 0) && psx_light_tunnel(&light, 3));        /* the RAMP context's block, whatever the rows say */
    CHECK(!psx_light_tunnel(&light, 4));                                      /* and not the rest, whatever the rows say */
    CHECK_EQ_INT(psx_light_at(&light, 0, 0), 153);                            /* even node: colour 128 -> T 96 -> 0.6 */
    CHECK_EQ_INT(psx_light_at(&light, 0, 8), 153);                            /* every point of the row the same: the LGT is not read */
    CHECK_EQ_INT(psx_light_at(&light, 1, 0), 99);                             /* odd node: colour 60 -> T 62 -> 0.3875 */
    CHECK_EQ_INT(psx_light_at(&light, 4, 0), 255);                            /* outside: the lit road is 1 */
    CHECK_EQ_INT(psx_light_at(&light, 4, 3), 51);                             /* LGT 0 is 0.2, not black */
    CHECK_EQ_INT(psx_light_at(&light, 4, 4), 210);                            /* LGT 200: T 32 + 100 = 132, 0.825 */
    CHECK_EQ_INT(psx_light_byte(&light, 4, 4), 200);
    CHECK_EQ_INT(psx_light_byte(&light, 5, 4), 200);                          /* the last node is not light: the one before */
    CHECK_EQ_INT(psx_light_byte(&light, 4 + 6, 4), 200);                      /* the ring wraps */
    CHECK_EQ_INT(psx_light_byte(NULL, 1, 1), PSX_LIGHT_FULL);
    CHECK(!psx_light_set_script(&light, NULL));
}

/* inside a tunnel (a lit wall foot with 0s beyond it, on either side) the road's own points are dimmed to PSX_LIGHT_TUNNEL_ROAD: the PS1
 * shows the road dark there although the file says 255; a darker byte stays, the open road and a bridge's rows are untouched */
static void test_tunnel_road(void)
{
    static uint8_t rows[5 * PSX_LIGHT_POINTS] = {
        255, 255, 255, 0, 0, 0, 255, 150, 0, 0, 0,        /* tunnel, both sides */
        255, 255, 255, 0, 228, 228, 255, 255, 0, 218, 218, /* only a degenerate 0 at point 3 (CY2's open road): open */
        255, 255, 10, 0, 0, 0, 255, 0, 0, 0, 0,          /* under a bridge: foot dark */
        100, 100, 126, 0, 0, 0, 100, 128, 0, 0, 0,       /* a tunnel with a road darker than the dim already */
        0,
    };
    const PsxLight light = { .data = rows, .nodes = 5 };
    CHECK_EQ_INT(psx_light_at(&light, 0, 0), PSX_LIGHT_TUNNEL_ROAD);
    CHECK_EQ_INT(psx_light_at(&light, 0, 6), PSX_LIGHT_TUNNEL_ROAD);
    CHECK_EQ_INT(psx_light_at(&light, 0, 1), PSX_LIGHT_TUNNEL_ROAD);
    CHECK_EQ_INT(psx_light_at(&light, 0, 2), PSX_LIGHT_WALL);            /* the wall's foot (the shoulder's far edge) is capped at half light... */
    CHECK_EQ_INT(psx_light_at(&light, 0, 7), PSX_LIGHT_WALL);            /* ...on both sides (150 -> 128) */
    CHECK_EQ_INT(psx_light_at(&light, 3, 2), 126);                       /* a foot under the cap stays */
    CHECK_EQ_INT(psx_light_at(&light, 1, 2), 255);                       /* and an open road's foot is its own */
    CHECK(psx_light_tunnel(&light, 0) && psx_light_tunnel(&light, 3));   /* the rows that are a tunnel's */
    CHECK(!psx_light_tunnel(&light, 1) && !psx_light_tunnel(&light, 2));
    CHECK(!psx_light_tunnel(NULL, 0));
    CHECK_EQ_INT(psx_light_at(&light, 1, 0), 255);
    CHECK_EQ_INT(psx_light_at(&light, 1, 1), 255);
    CHECK_EQ_INT(psx_light_at(&light, 2, 1), 255);
    CHECK_EQ_INT(psx_light_at(&light, 3, 0), 100);
    CHECK_EQ_INT(psx_light_at(&light, 3, 1), 100);
}

/* a track run the other way round (tri_reverse.h): the light is the same places' light, asked at the new node (j is the file's (N - j) mod N) and
 * the new side (the two chains of a row swap), with and without the script, in a tunnel and out */
static void test_reversed(void)
{
    enum { N = 12 };
    uint8_t rows[N * PSX_LIGHT_POINTS];
    for (int n = 0; n < N; n++)
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) rows[n * PSX_LIGHT_POINTS + p] = (uint8_t)(7 * n + 11 * p + 3);
    for (int n = 4; n < 8; n++) {   /* a tunnel: a lit left wall foot with 0s beyond it, on the file's left side only */
        uint8_t *row = rows + n * PSX_LIGHT_POINTS;
        row[7] = 200, row[9] = 0, row[10] = 0;
    }
    PsxLgs lgs = { { 0 }, { 128, 128, 128 }, { 100, 100, 100 }, 1, { { 0 } } };
    lgs.contexts[0] = (PsxLgsContext){ 1, 1, PSX_LGS_RAMP, { 128, 128, 128 }, { 60, 60, 60 } };   /* block 1: nodes 4..7 */
    for (int scripted = 0; scripted < 2; scripted++) {
        PsxLight forward = { .data = rows, .nodes = N }, back = { .data = rows, .nodes = N };
        if (scripted) CHECK(psx_light_set_script(&forward, &lgs) && psx_light_set_script(&back, &lgs));
        CHECK(psx_light_set_reversed(&back, 1));
        for (uint32_t j = 0; j < N; j++) {
            const uint32_t old = (N - j) % N;
            CHECK_EQ_INT(psx_light_old_node(&back, j), old);
            CHECK_EQ_INT(psx_light_old_node(&back, j + N), old);   /* the ring wraps */
            CHECK_EQ_INT(psx_light_tunnel(&back, j), psx_light_tunnel(&forward, old));
            CHECK_EQ_INT(psx_light_node(&back, j), psx_light_node(&forward, old));
            for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
                const int side = p == 0 ? 0 : (p + 4) % 10 + 1;
                CHECK_EQ_INT(psx_light_at(&back, j, p), psx_light_at(&forward, old, side));
                CHECK_EQ_INT(psx_light_byte(&back, j, p), psx_light_byte(&forward, old, side));
            }
        }
        CHECK_EQ_INT(psx_light_at(&back, 0, 7), psx_light_at(&forward, 0, 2));   /* node 0 stays, its sides swap */
    }
    PsxLight none = { 0 };
    CHECK(!psx_light_set_reversed(&none, 1));
    CHECK_EQ_INT(psx_light_old_node(NULL, 5), 5);
}

int main(void)
{
    test_tunnel_road();
    test_script();
    test_tunnel_wall_floor();
    test_parse_and_lookup();
    test_rejected_files();
    test_reversed();
    return TEST_REPORT();
}
