/* The tunnel lamps' table (psx_lamps.h): which texture of which track holds a lamp and which of its colours stay bright, and the
 * darkening tables with those colours let through. */
#include "game/race/psx_lamps.h"
#include "test.h"

#include <math.h>
#include <string.h>

static void test_lookup(void)
{
    const PsxLamp *lamps = NULL;
    int n = psx_lamps_for("CY1", &lamps);
    CHECK_EQ_INT(n, 1);
    CHECK_EQ_INT(lamps[0].slot, 38);
    CHECK_EQ_INT(lamps[0].count, 5);
    CHECK_EQ_INT(lamps[0].idx[0], 0);
    CHECK_EQ_INT(lamps[0].idx[4], 67);
    CHECK_EQ_INT(psx_lamps_for("cy3", &lamps), 2);   /* the case of the name does not matter */
    CHECK_EQ_INT(lamps[0].slot, 20);
    CHECK_EQ_INT(lamps[1].slot, 19);
    CHECK_EQ_INT(psx_lamps_for("TR4", &lamps), 1);
    CHECK_EQ_INT(lamps[0].slot, 20);
    CHECK_EQ_INT(psx_lamps_for("CY2", &lamps), 1);
    CHECK_EQ_INT(psx_lamps_for("AL1", &lamps), 0);   /* a frieze only */
    CHECK_EQ_INT(psx_lamps_for("AL3", &lamps), 0);
    CHECK_EQ_INT(psx_lamps_for("TR1", &lamps), 0);
    CHECK_EQ_INT(psx_lamps_for(NULL, &lamps), 0);
    CHECK_EQ_INT(psx_lamps_for("CY1", NULL), 1);
}

/* every lamp's colours fit its table and no slot is named twice for a track */
static void test_table_sane(void)
{
    static const char *const TRACKS[] = { "CY1", "CY2", "CY3", "TR4" };
    for (size_t t = 0; t < sizeof TRACKS / sizeof *TRACKS; t++) {
        const PsxLamp *lamps = NULL;
        const int n = psx_lamps_for(TRACKS[t], &lamps);
        for (int i = 0; i < n; i++) {
            CHECK(lamps[i].count > 0 && lamps[i].count <= PSX_LAMP_COLOURS);
            CHECK(lamps[i].slot >= 0 && lamps[i].slot < 255);
            for (int j = i + 1; j < n; j++) CHECK(lamps[i].slot != lamps[j].slot);
        }
    }
}

static void test_tables(void)
{
    enum { LEVELS = 4 };
    uint8_t lit[LEVELS][256], out[LEVELS][256];
    for (int l = 0; l < LEVELS; l++)
        for (int c = 0; c < 256; c++) lit[l][c] = (uint8_t)(c / (l + 1));   /* each level darker: level 0 is the colour itself */
    uint8_t exempt[256] = { 0 };
    exempt[40] = exempt[200] = 1;
    psx_lamp_tables(exempt, (const uint8_t (*)[256])lit, LEVELS, out);
    for (int l = 0; l < LEVELS; l++) {
        CHECK_EQ_INT(out[l][40], 40);     /* the lamp's colours as they are, at every level */
        CHECK_EQ_INT(out[l][200], 200);
        for (int c = 0; c < 256; c++)
            if (c != 40 && c != 200) CHECK_EQ_INT(out[l][c], lit[l][c]);   /* the rest darkened as the world is */
    }
}

/* a lamp's own colours and the ones only its rectangle shows stay bright; a colour the rest of the texture uses too does not */
static void test_exempt(void)
{
    enum { W = 6, H = 4 };
    uint8_t px[W * H];
    memset(px, 9, sizeof px);                     /* the wall: colour 9 everywhere... */
    px[1 * W + 1] = 50, px[1 * W + 2] = 51;       /* ...the lamp's glow inside its rectangle (x 1..3, y 1..2) */
    px[2 * W + 3] = 9;                            /* a texel of the wall's colour inside the rectangle too */
    px[0 * W + 5] = 51;                           /* and 51 outside it: not the rectangle's alone */
    const PsxLamp lamp = { 7, 1, { 77 }, { 1, 1, 3, 2 } };
    uint8_t exempt[256];
    psx_lamp_exempt(&lamp, px, W, H, exempt);
    CHECK_EQ_INT(exempt[77], 1);   /* listed, though the texture does not show it */
    CHECK_EQ_INT(exempt[50], 1);   /* only in the rectangle */
    CHECK_EQ_INT(exempt[51], 0);   /* in the rectangle and out of it */
    CHECK_EQ_INT(exempt[9], 0);    /* the wall */
    CHECK_EQ_INT(exempt[0], 0);
    psx_lamp_exempt(&lamp, NULL, 0, 0, exempt);   /* no texture: the list alone */
    CHECK_EQ_INT(exempt[77], 1);
    CHECK_EQ_INT(exempt[50], 0);
}

/* a car is under a lamp for PSX_LAMP_DUTY of each 4-node segment, centred on the lamp's place in it (v, 0..1 along the segment), and
 * nowhere else; the steps are hard, there is no ramp */
static void test_lit(void)
{
    const double v = 0.9;
    int lit = 0;
    for (int i = 0; i < 400; i++) lit += psx_lamp_lit(1100.0 + i * 0.01, v);   /* one segment in 0.01 nodes (4 nodes) */
    CHECK(lit >= 215 && lit <= 225);                                              /* 55% of 400 */
    CHECK(psx_lamp_lit(1100.0 + 4 * 0.9, v));                                     /* on the lamp */
    CHECK(psx_lamp_lit(1100.0 + 4 * 0.9 + 4 * 0.2, v));                           /* inside the duty */
    CHECK(!psx_lamp_lit(1100.0 + 4 * 0.9 - 4 * 0.4, v));                          /* between lamps */
    CHECK(psx_lamp_lit(1104.0 + 0.1, v));                                         /* the lit part wraps into the next segment (0.9 + 0.275 > 1) */
    CHECK(psx_lamp_lit(-0.1, v) == psx_lamp_lit(4 * 100 - 0.1, v));               /* a ring node before 0 */
    CHECK_EQ_INT(psx_lamp_lit(1100.0, v), psx_lamp_lit(1100.0 + 4 * 25, v));      /* the same in every segment */
}

/* the lamp hangs where its rectangle is along the texture's u, which runs against the nodes: 1 - u */
static void test_place(void)
{
    const PsxLamp *lamps = NULL;
    psx_lamps_for("CY1", &lamps);   /* x 33..41 of an 80 wide texture */
    CHECK(fabs(psx_lamp_place(&lamps[0], 80) - (1.0 - 37.5 / 80)) < 1e-9);   /* about 0.53, not the 0.91 of its y */
    psx_lamps_for("TR4", &lamps);   /* the full width: mid-segment */
    CHECK(fabs(psx_lamp_place(&lamps[0], 128) - 0.5) < 1e-9);
    CHECK(fabs(psx_lamp_place(NULL, 80) - 0.5) < 1e-9);
}

/* a ring that is not a power of two (CY1's 1920) wraps by a modulo: a mask read node 1200 as 1072 */
static void test_ring(void)
{
    CHECK_EQ_INT(psx_ring_node(1200, 1920), 1200);
    CHECK_EQ_INT(psx_ring_node(1920, 1920), 0);
    CHECK_EQ_INT(psx_ring_node(1921 + 1920, 1920), 1);
    CHECK_EQ_INT(psx_ring_node(-1, 1920), 1919);
    CHECK_EQ_INT(psx_ring_node(505, 1760), 505);
    CHECK_EQ_INT(psx_ring_node(7, 0), 0);
}

/* CY1's script: a ramped (tunnel) context at segments 266..292 and fixed ones (underpasses) at 125 and 127; the car's ratio is the ambient's outside
 * a tunnel and the lamp's steps inside, and it is the one of the place, wherever the ring's mask would have put it */
static void test_car_ratio(void)
{
    PsxLgs lgs = { { 0 }, { 128, 128, 128 }, { 128, 128, 128 }, 3, { { 0 } } };
    lgs.contexts[0] = (PsxLgsContext){ 266, 292, PSX_LGS_RAMP, { 128, 115, 96 }, { 60, 60, 60 } };
    lgs.contexts[1] = (PsxLgsContext){ 125, 125, PSX_LGS_FIXED, { 128, 115, 96 }, { 0, 0, 0 } };
    lgs.contexts[2] = (PsxLgsContext){ 127, 127, PSX_LGS_FIXED, { 128, 115, 96 }, { 0, 0, 0 } };
    const float tint[3] = { 1, 1, 1 };
    float r[3];
    psx_light_car_ratio(&lgs, 1920, 1200, 1200.3, 0.53, tint, 16, r);   /* open road, segment 300 (masked: 1072, segment 268, the tunnel) */
    CHECK(r[0] == 1.0f && r[1] == 1.0f && r[2] == 1.0f);
    psx_light_car_ratio(&lgs, 1920, 1160, 1160.0 + 4 * 0.53, 0.53, tint, 16, r);   /* the tunnel's last segments (290; masked: 1032, segment 258, no context) */
    CHECK(r[0] == PSX_TUNNEL_LIT[0] && r[2] == PSX_TUNNEL_LIT[2]);
    psx_light_car_ratio(&lgs, 1920, 1100, 1100.0 + 4 * 0.53, 0.53, tint, 16, r);   /* in the tunnel, on the lamp */
    CHECK(r[0] == PSX_TUNNEL_LIT[0] && r[1] == PSX_TUNNEL_LIT[1] && r[2] == PSX_TUNNEL_LIT[2]);
    psx_light_car_ratio(&lgs, 1920, 1100, 1100.0 + 4 * 0.03, 0.53, tint, 16, r);   /* between lamps */
    CHECK(r[0] == PSX_TUNNEL_UNLIT[0] && r[2] == PSX_TUNNEL_UNLIT[2]);
    psx_light_car_ratio(&lgs, 1920, 1100 + 1920, 1100.0 + 1920 + 4 * 0.03, 0.53, tint, 16, r);   /* a lap on: the same place */
    CHECK(r[0] == PSX_TUNNEL_UNLIT[0]);
    psx_light_car_ratio(&lgs, 1920, 1100, 1100.0, -1.0, tint, 16, r);   /* a track with no lamps: the ambient ramp, no steps */
    CHECK(r[0] != PSX_TUNNEL_UNLIT[0] && r[0] <= 1.0f && r[2] < 1.0f);
    psx_light_car_ratio(&lgs, 1920, 500, 500.0, 0.53, tint, 16, r);   /* segment 125: the underpass, fixed (128,115,96) */
    CHECK(r[0] == 1.0f && r[2] == 0.75f);
}

int main(void)
{
    test_place();
    test_ring();
    test_car_ratio();
    test_lit();
    test_lookup();
    test_table_sane();
    test_tables();
    test_exempt();
    return TEST_REPORT();
}
