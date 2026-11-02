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
    CHECK(!street_lamp_for("TR5", &l) && !street_lamp_for(NULL, &l));
    /* Coastal's first track: the lamp with the arm, board 10 (87 x 128; its amber lens is at the arm's end, rows 7 and 8) */
    CHECK(street_lamp_for("CL1", &l));
    CHECK(street_lamp_for("cl1r", &l));
    CHECK_EQ_INT(l.scenery, 10);
    CHECK_EQ_INT(l.width, 87);
    CHECK_EQ_INT(l.height, 128);
    CHECK(l.lens_rect[0] >= 70 && l.lens_rect[2] < 87 && l.lens_rect[1] == 7 && l.lens_rect[3] == 8);
    CHECK(!street_lamp_for("CL2", &l) && !street_lamp_for("AL1", &l));
    /* Autumn Valley (TR2): the floodlight pole, board 5 (38 x 148; four heads over rows 0..21, the pole under them) */
    CHECK(street_lamp_for("TR2", &l));
    CHECK_EQ_INT(l.scenery, 5);
    CHECK_EQ_INT(l.width, 38);
    CHECK_EQ_INT(l.height, 148);
    CHECK(l.lens_rect[3] <= 21 && l.halo_y >= l.lens_rect[3]);
    CHECK(!street_lamp_for("TR1", &l));
}

/* Vertigo Ridge (TR3): two boards, the lantern post (16 x 150) and the truss with its hanging bulb (144 x 82, the bulb at its foot: the halo goes round it) */
static void test_vertigo_ridge_has_two_lamps(void)
{
    StreetLamp l[STREET_LAMPS_MAX];
    CHECK_EQ_INT(street_lamps_for("TR3", l, STREET_LAMPS_MAX), 2);
    CHECK_EQ_INT(l[0].scenery, 33);
    CHECK_EQ_INT(l[0].width, 16);
    CHECK_EQ_INT(l[1].scenery, 34);
    CHECK_EQ_INT(l[1].width, 144);
    CHECK_EQ_INT(l[1].height, 82);
    CHECK_EQ_INT(street_lamps_for("CY1", l, STREET_LAMPS_MAX), 1);
    CHECK_EQ_INT(street_lamps_for("AL1", l, STREET_LAMPS_MAX), 0);
    CHECK_EQ_INT(street_lamps_for("CY2", l, STREET_LAMPS_MAX), 1);
    CHECK_EQ_INT(l[0].scenery, 7); CHECK_EQ_INT(l[0].width, 65); CHECK_EQ_INT(l[0].height, 128);
    CHECK_EQ_INT(street_lamps_for("TR7", l, STREET_LAMPS_MAX), 1);
    CHECK_EQ_INT(l[0].scenery, 28); CHECK_EQ_INT(l[0].width, 64); CHECK_EQ_INT(l[0].height, 127);
    CHECK_EQ_INT(street_lamps_for("CY3", l, STREET_LAMPS_MAX), 1);
    CHECK_EQ_INT(l[0].scenery, 7); CHECK_EQ_INT(l[0].width, 64); CHECK_EQ_INT(l[0].height, 128);
    enum { BW = 144, BH = 82 };
    uint8_t px[BW * BH];
    memset(px, 255, sizeof px);
    for (int y = 69; y <= 80; y++) memset(px + y * BW + 69, 40, 7);   /* the bulb */
    const uint8_t entry[STREET_LAMP_ENTRIES] = { 200, 201, 202 };
    const StreetLampPaint p = street_lamp_paint(&l[1], px, BW, BH, entry);
    CHECK_EQ_INT((int)p.lens, 7 * 12);
    int above = 0, beside = 0;
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            if (px[y * BW + x] >= 201) above += y < 69, beside += y >= 69 && (x < 69 || x > 75);
    CHECK(above > 0 && beside > 0);   /* the halo is round the bulb, not only under it */
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

int main(void)
{
    test_the_lamp_of_each_track();
    test_vertigo_ridge_has_two_lamps();
    test_the_lens_and_the_halo();
    test_painting_again_and_the_wrong_board();
    return TEST_REPORT();
}
