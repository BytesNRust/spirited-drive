/* The tunnel lamps' table (tunnel_lamp_textures.h): which texture of which track holds a lamp and which of its colours stay bright, and the
 * darkening tables with those colours let through. */
#include "game/race/tunnel_lamp_textures.h"
#include "test.h"

#include <string.h>

static void test_lookup(void)
{
    const TunnelLampTexture *lamps = NULL;
    int n = tunnel_lamp_textures_for("CY1", &lamps);
    CHECK_EQ_INT(n, 1);
    CHECK_EQ_INT(lamps[0].slot, 38);
    CHECK_EQ_INT(lamps[0].count, 5);
    CHECK_EQ_INT(lamps[0].idx[0], 0);
    CHECK_EQ_INT(lamps[0].idx[4], 67);
    CHECK_EQ_INT(tunnel_lamp_textures_for("cy3", &lamps), 2);   /* the case of the name does not matter */
    CHECK_EQ_INT(lamps[0].slot, 20);
    CHECK_EQ_INT(lamps[1].slot, 19);
    CHECK_EQ_INT(tunnel_lamp_textures_for("TR4", &lamps), 1);
    CHECK_EQ_INT(lamps[0].slot, 20);
    CHECK_EQ_INT(tunnel_lamp_textures_for("TR3", &lamps), 2);   /* Vertigo Ridge: the portal's two bulbs (bytes 19 and 21) */
    CHECK(lamps[0].slot == 19 && lamps[1].slot == 21);
    CHECK_EQ_INT(tunnel_lamp_textures_for("CY2", &lamps), 1);
    CHECK_EQ_INT(tunnel_lamp_textures_for("AL1", &lamps), 0);   /* a frieze only */
    CHECK_EQ_INT(tunnel_lamp_textures_for("AL3", &lamps), 0);
    CHECK_EQ_INT(tunnel_lamp_textures_for("TR1", &lamps), 0);
    CHECK_EQ_INT(tunnel_lamp_textures_for(NULL, &lamps), 0);
    CHECK_EQ_INT(tunnel_lamp_textures_for("CY1", NULL), 1);
}

/* every lamp's colours fit its table and no slot is named twice for a track */
static void test_table_sane(void)
{
    static const char *const TRACKS[] = { "CY1", "CY2", "CY3", "TR4" };
    for (size_t t = 0; t < sizeof TRACKS / sizeof *TRACKS; t++) {
        const TunnelLampTexture *lamps = NULL;
        const int n = tunnel_lamp_textures_for(TRACKS[t], &lamps);
        for (int i = 0; i < n; i++) {
            CHECK(lamps[i].count > 0 && lamps[i].count <= TUNNEL_LAMP_TEXTURE_COLOURS);
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
    tunnel_lamp_texture_tables(exempt, (const uint8_t (*)[256])lit, LEVELS, out);
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
    const TunnelLampTexture lamp = { 7, 1, { 77 }, { 1, 1, 3, 2 }, NULL };
    uint8_t exempt[256];
    tunnel_lamp_texture_exempt(&lamp, px, W, H, exempt);
    CHECK_EQ_INT(exempt[77], 1);   /* listed, though the texture does not show it */
    CHECK_EQ_INT(exempt[50], 1);   /* only in the rectangle */
    CHECK_EQ_INT(exempt[51], 0);   /* in the rectangle and out of it */
    CHECK_EQ_INT(exempt[9], 0);    /* the wall */
    CHECK_EQ_INT(exempt[0], 0);
    tunnel_lamp_texture_exempt(&lamp, NULL, 0, 0, exempt);   /* no texture: the list alone */
    CHECK_EQ_INT(exempt[77], 1);
    CHECK_EQ_INT(exempt[50], 0);
}

/* the night's lamp texels: each colour that has an entry of its own is changed to it, the others stay */
static void test_recolour(void)
{
    uint8_t pixels[6] = { 27, 33, 27, 255, 170, 5 }, remap[256];
    for (int c = 0; c < 256; c++) remap[c] = (uint8_t)c;
    remap[27] = 144, remap[170] = 143;
    CHECK_EQ_INT((int)tunnel_lamp_texture_recolour(pixels, 6, remap), 3);
    CHECK(pixels[0] == 144 && pixels[1] == 33 && pixels[2] == 144 && pixels[3] == 255 && pixels[4] == 143 && pixels[5] == 5);
    CHECK_EQ_INT((int)tunnel_lamp_texture_recolour(NULL, 6, remap), 0);
    CHECK_EQ_INT((int)tunnel_lamp_texture_recolour(pixels, 0, remap), 0);
}

static void test_recolour_rect(void)
{
    /* 4 x 3: the lamp's rectangle is x 1-2, y 1-1; colour 27 is the glare and also the wall around it */
    uint8_t pixels[12] = { 27, 27, 27, 27,  27, 27, 33, 27,  27, 27, 27, 27 }, remap[256];
    const TunnelLampTexture lamp = { 0, 1, { 27 }, { 1, 1, 2, 1 }, NULL };
    for (int c = 0; c < 256; c++) remap[c] = (uint8_t)c;
    remap[27] = 144;
    CHECK_EQ_INT((int)tunnel_lamp_texture_recolour_rect(&lamp, pixels, 4, 3, remap), 1);
    CHECK(pixels[5] == 144 && pixels[0] == 27 && pixels[4] == 27 && pixels[6] == 33 && pixels[7] == 27 && pixels[9] == 27);
    CHECK_EQ_INT((int)tunnel_lamp_texture_recolour_rect(&lamp, NULL, 4, 3, remap), 0);
}

int main(void)
{
    test_lookup();
    test_table_sane();
    test_tables();
    test_exempt();
    test_recolour();
    test_recolour_rect();
    return TEST_REPORT();
}
