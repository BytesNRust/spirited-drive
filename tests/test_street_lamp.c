/* The street lamp's night flare (street_lamp.h) on a made-up board: the lens, the halo's dither, what stays, idempotence, the other tracks. */
#include "game/race/street_lamp.h"
#include "tests/test.h"

#include <string.h>

enum { W = 64, H = 128, HOUSING = 96, POLE = 100, A = 59, B = 55, C = 60 };

static void board(uint8_t px[W * H])
{
    memset(px, 255, W * H);
    for (int y = 0; y < 7; y++) memset(px + y * W + 40, HOUSING, 24);   /* the head's housing */
    for (int y = 0; y < H; y++) memset(px + y * W + 2, POLE, 5);        /* the post */
    for (int x = 48; x <= 62; x++) px[7 * W + x] = A, px[8 * W + x] = B, px[9 * W + x] = x % 2 ? B : C;   /* the lens */
}

static void test_the_lamp_of_each_track(void)
{
    StreetLamp l;
    CHECK(street_lamp_for("CY1", &l));
    CHECK(street_lamp_for("cy1", &l));
    CHECK_EQ_INT(l.scenery, 10);
    CHECK_EQ_INT(l.width, 64);
    CHECK(!street_lamp_for("CY2", &l) && !street_lamp_for("TR5", &l) && !street_lamp_for(NULL, &l));
}

static void test_the_lens_and_the_halo(void)
{
    StreetLamp l;
    street_lamp_for("CY1", &l);
    uint8_t px[W * H], before[W * H];
    board(px);
    memcpy(before, px, sizeof px);
    const uint8_t entry[STREET_LAMP_ENTRIES] = { 200, 201, 202 };
    const StreetLampPaint p = street_lamp_paint(&l, px, W, H, entry);
    CHECK_EQ_INT((int)p.lens, 45);   /* the lens's 3 x 15 texels */
    CHECK(p.halo > 10);
    int lens = 0, core = 0, rim = 0;
    for (int i = 0; i < W * H; i++) {
        if (px[i] == before[i]) continue;
        const int x = i % W, y = i / W;
        if (px[i] == 200) {
            lens++;
            CHECK(y >= 7 && y <= 9 && x >= 48 && x <= 62);   /* only the lens's texels */
        } else {
            CHECK(before[i] == 255);   /* the halo only takes transparent texels */
            CHECK(px[i] == 201 || px[i] == 202);
            core += px[i] == 201, rim += px[i] == 202;
            CHECK(y > 9 || x < 40);    /* under the lens: the housing above stays */
        }
    }
    CHECK_EQ_INT(lens, 45);
    CHECK(core > 0 && rim > 0);
    CHECK_EQ_INT((int)(p.halo), core + rim);
    CHECK_EQ_INT(px[4 * W + 45], HOUSING);   /* the housing and the post are as they were */
    CHECK_EQ_INT(px[60 * W + 4], POLE);
    CHECK_EQ_INT(px[100 * W + 50], 255);     /* far from the lens: still transparent */
}

static void test_painting_again_and_the_wrong_board(void)
{
    StreetLamp l;
    street_lamp_for("CY1", &l);
    uint8_t px[W * H], once[W * H];
    board(px);
    const uint8_t entry[STREET_LAMP_ENTRIES] = { 200, 201, 202 };
    street_lamp_paint(&l, px, W, H, entry);
    memcpy(once, px, sizeof px);
    const StreetLampPaint again = street_lamp_paint(&l, px, W, H, entry);
    CHECK_EQ_INT((int)again.lens, 0);
    CHECK_EQ_INT((int)again.halo, 0);
    CHECK(memcmp(px, once, sizeof px) == 0);
    uint8_t small[32 * 32] = { 0 };
    const StreetLampPaint none = street_lamp_paint(&l, small, 32, 32, entry);
    CHECK_EQ_INT((int)(none.lens + none.halo), 0);
    CHECK_EQ_INT((int)street_lamp_paint(&l, NULL, W, H, entry).lens, 0);
}

static void test_the_lamp_posts_nodes(void)
{
    StreetLamp l;
    street_lamp_for("CY1", &l);
    TriDescriptor d[3] = { { 0 } };
    d[0].type = 6, d[0].resource_id = 40;   /* the lamp */
    d[1].type = 1, d[1].resource_id = 40;   /* a model of the same resource number: not a board */
    d[2].type = 4, d[2].resource_id = 12;   /* another board */
    TriScenery sc[6] = { { 0 } };
    sc[0].node = 10, sc[0].descriptor = 0;
    sc[1].node = 20, sc[1].descriptor = 1;
    sc[2].node = 30, sc[2].descriptor = 2;
    sc[3].node = 40, sc[3].descriptor = 0;
    sc[4].node = -1, sc[4].descriptor = 0;   /* no node */
    sc[5].node = 50, sc[5].descriptor = 9;   /* a descriptor that is not there */
    uint32_t nodes[8];
    CHECK_EQ_INT((int)street_lamp_nodes(&l, d, 3, sc, 6, nodes, 8), 2);
    CHECK_EQ_INT((int)nodes[0], 10);
    CHECK_EQ_INT((int)nodes[1], 40);
    CHECK_EQ_INT((int)street_lamp_nodes(&l, d, 3, sc, 6, nodes, 1), 1);   /* up to the room */
    CHECK_EQ_INT((int)street_lamp_nodes(NULL, d, 3, sc, 6, nodes, 8), 0);
}

int main(void)
{
    test_the_lamp_posts_nodes();
    test_the_lamp_of_each_track();
    test_the_lens_and_the_halo();
    test_painting_again_and_the_wrong_board();
    return TEST_REPORT();
}
