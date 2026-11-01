/* The texel-scoped repaint of a car's bank (car_repaint.h) on a synthetic bank: the body textures change, nothing else does. */
#include "game/race/car_repaint.h"
#include "game/race/car_mask.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { SIDE = 8, SHARED = 10, ONLY = 11, TRIM = 12, GLASS = 3, TYRE_ONLY = 20, TEXELS = SIDE * SIDE };

typedef struct {
    uint8_t body[TEXELS], tyre[TEXELS], decal[TEXELS];
    uint8_t pal[768], xxx[768], pal_before[768];
    RepaintTexture tex[3];
    RepaintBank bank;
} Fixture;

static void set(uint8_t *pal, int entry, int r, int g, int b)
{
    pal[3 * entry] = (uint8_t)r, pal[3 * entry + 1] = (uint8_t)g, pal[3 * entry + 2] = (uint8_t)b;
}

/* body texture: entries SHARED (a silver grey the wheels share), ONLY, TRIM (a grey no pair covers) and the glass;
 * the tyre texture: SHARED and TYRE_ONLY; a decal texture marked as not body */
static void make(Fixture *f, int type)
{
    memset(f, 0, sizeof *f);
    const int s = type == 0x22 ? 2 : 0;
    for (int i = 0; i < TEXELS; i++) {
        f->body[i] = (uint8_t)(i % 4 == 0 ? SHARED : i % 4 == 1 ? ONLY : i % 4 == 2 ? TRIM : GLASS);
        f->tyre[i] = (uint8_t)(i % 2 ? SHARED : TYRE_ONLY);
        f->decal[i] = 0xff;
    }
    set(f->pal, SHARED, 196 >> s, 196 >> s, 196 >> s);
    set(f->pal, ONLY, 120 >> s, 120 >> s, 124 >> s);
    set(f->pal, TRIM, 90 >> s, 90 >> s, 94 >> s);
    set(f->pal, GLASS, 16 >> s, 16 >> s, 16 >> s);
    set(f->pal, TYRE_ONLY, 30 >> s, 30 >> s, 30 >> s);
    memcpy(f->xxx, f->pal, sizeof f->xxx);
    memcpy(f->pal_before, f->pal, sizeof f->pal);
    f->tex[0] = (RepaintTexture){ f->body, TEXELS, REPAINT_MAIN, 0, { 0 }, NULL };
    f->tex[1] = (RepaintTexture){ f->tyre, TEXELS, REPAINT_NONE, 0, { 0 }, NULL };
    f->tex[2] = (RepaintTexture){ f->decal, TEXELS, REPAINT_NONE, 0, { 0 }, NULL };
    f->bank = (RepaintBank){ f->tex, 3, { f->pal, f->xxx }, 2, type, type };
}

static int colour(const Fixture *f, const uint8_t *pal, int entry, int c)
{
    return f->bank.type == 0x22 ? pal[3 * entry + c] << 2 : pal[3 * entry + c];
}

static void test_paint(int type)
{
    Fixture f;
    make(&f, type);
    uint8_t tyre_before[TEXELS], decal_before[TEXELS];
    memcpy(tyre_before, f.tyre, sizeof tyre_before), memcpy(decal_before, f.decal, sizeof decal_before);
    const uint8_t blue[3] = { 30, 70, 200 };
    const int changed = car_repaint_paint(&f.bank, blue, 44);
    CHECK(changed > 0);
    CHECK_MEM(f.tyre, tyre_before, TEXELS);      /* the other textures are as they were */
    CHECK_MEM(f.decal, decal_before, TEXELS);
    /* the shared entry keeps its colour (the tyres use it) and the body's texels that showed it moved to a spare one that is blue */
    CHECK_MEM(f.pal + 3 * SHARED, f.pal_before + 3 * SHARED, 3);
    CHECK_MEM(f.pal + 3 * TYRE_ONLY, f.pal_before + 3 * TYRE_ONLY, 3);
    const int moved = f.body[0];
    CHECK(moved != SHARED && moved != ONLY && moved != TRIM && moved != GLASS);
    CHECK(colour(&f, f.pal, moved, 2) > colour(&f, f.pal, moved, 0) + 40);
    CHECK_MEM(f.xxx + 3 * moved, f.pal + 3 * moved, 3);   /* the second palette follows */
    CHECK_MEM(f.xxx + 3 * ONLY, f.pal + 3 * ONLY, 3);
    /* the entry only the body uses was recoloured in place; the trim grey is blue too: no speckle */
    CHECK(f.body[1] == ONLY && colour(&f, f.pal, ONLY, 2) > colour(&f, f.pal, ONLY, 0) + 40);
    CHECK(colour(&f, f.pal, f.body[2], 2) > colour(&f, f.pal, f.body[2], 0) + 40);
    CHECK_EQ_INT(f.body[3], GLASS);                       /* the glass is under the cutoff and stays */
    CHECK_MEM(f.pal + 3 * GLASS, f.pal_before + 3 * GLASS, 3);
    /* every other palette entry is as before */
    for (int e = 0; e < 256; e++) {
        if (e == ONLY || e == TRIM || e == f.body[0] || e == f.body[2]) continue;
        CHECK_MEM(f.pal + 3 * e, f.pal_before + 3 * e, 3);
    }
}

static void test_pairs(void)
{
    Fixture f;
    make(&f, 0x24);
    /* the 196 grey -> red; the 120 grey -> a dark red: the 90 grey (no pair) takes the pair nearest in lightness */
    const uint16_t grey196 = (uint16_t)(24 | 24 << 5 | 24 << 10), grey120 = (uint16_t)(15 | 15 << 5 | 15 << 10);
    const CarLiveryPair pairs[2] = { { grey196, (uint16_t)(28 | 3 << 5 | 3 << 10) }, { grey120, (uint16_t)(14 | 1 << 5 | 1 << 10) } };
    CHECK(car_repaint_pairs(&f.bank, pairs, 2, 44) > 0);
    CHECK_MEM(f.pal + 3 * SHARED, f.pal_before + 3 * SHARED, 3);   /* the wheels' grey is not touched */
    CHECK(colour(&f, f.pal, f.body[0], 0) > colour(&f, f.pal, f.body[0], 1) + 100);   /* red */
    CHECK(colour(&f, f.pal, f.body[2], 0) > colour(&f, f.pal, f.body[2], 1) + 40);    /* the unpaired grey red too (90 is near 120) */
    CHECK_EQ_INT(f.body[3], GLASS);
}

/* a texel between two pairs takes the colour between theirs, by lightness, not the nearer pair's: no streaks across a dark body */
static void test_pairs_between(void)
{
    Fixture f;
    make(&f, 0x24);
    const uint16_t grey64 = (uint16_t)(8 | 8 << 5 | 8 << 10), grey120 = (uint16_t)(15 | 15 << 5 | 15 << 10);
    const CarLiveryPair pairs[2] = { { grey64, (uint16_t)(7 | 1 << 5 | 1 << 10) }, { grey120, (uint16_t)(14 | 1 << 5 | 1 << 10) } };
    CHECK(car_repaint_pairs(&f.bank, pairs, 2, 44) > 0);
    const int r = colour(&f, f.pal, f.body[2], 0);   /* the trim grey (88): 0.43 of the way from 56 to 112 */
    CHECK(r > 70 && r < 92);
    CHECK(colour(&f, f.pal, f.body[1], 0) == 112);   /* the 120 grey is its pair's exactly */
}

/* a texture's kept rectangle (the plate) is left as it was, and an entry it shows is not recoloured where it is */
static void test_keep(void)
{
    Fixture f;
    make(&f, 0x24);
    f.tex[0].width = SIDE;
    f.tex[0].keep[0][0] = 0, f.tex[0].keep[0][1] = 0, f.tex[0].keep[0][2] = 2, f.tex[0].keep[0][3] = 2;   /* texels 0, 1, 8, 9 */
    f.tex[0].keep[1][0] = 5, f.tex[0].keep[1][1] = 6, f.tex[0].keep[1][2] = 2, f.tex[0].keep[1][3] = 1;   /* and 53, 54 */
    uint8_t body_before[TEXELS];
    memcpy(body_before, f.body, sizeof body_before);
    const uint8_t blue[3] = { 30, 70, 200 };
    CHECK(car_repaint_paint(&f.bank, blue, 44) > 0);
    CHECK(f.body[53] == body_before[53] && f.body[54] == body_before[54]);   /* a second rectangle */
    CHECK(f.body[0] == body_before[0] && f.body[1] == body_before[1] && f.body[8] == body_before[8] && f.body[9] == body_before[9]);
    CHECK_MEM(f.pal + 3 * ONLY, f.pal_before + 3 * ONLY, 3);   /* the plate still shows ONLY: its colour stays */
    CHECK(f.body[17] != ONLY && colour(&f, f.pal, f.body[17], 2) > colour(&f, f.pal, f.body[17], 0) + 40);   /* the rest is blue */
    CHECK(colour(&f, f.pal, f.body[2], 2) > colour(&f, f.pal, f.body[2], 0) + 40);
}

/* a colour far darker than the darkest pair (a louvre slat) is left, not mapped to a violet streak */
static void test_pairs_dark(void)
{
    Fixture f;
    make(&f, 0x24);
    set(f.pal, TRIM, 40, 40, 44);   /* TRIM: 40, the lowest pair is 120 */
    memcpy(f.xxx, f.pal, sizeof f.xxx);
    const uint16_t grey120 = (uint16_t)(15 | 15 << 5 | 15 << 10);
    const CarLiveryPair pairs[1] = { { grey120, (uint16_t)(14 | 1 << 5 | 1 << 10) } };
    car_repaint_pairs(&f.bank, pairs, 1, 20);
    CHECK_EQ_INT(f.body[2], TRIM);
    CHECK_MEM(f.pal + 3 * TRIM, f.pal_before + 3 * TRIM, 0);
    CHECK_EQ_INT(colour(&f, f.pal, TRIM, 0), 40);
}

/* glass dithered with the paint's own colour (the Corvette's hatch) stays: a texel with half its neighbours not paint is left */
static void test_dither(void)
{
    enum { W = 16, H = 8 };
    uint8_t pal[768] = { 0 }, xxx[768], body[W * H];
    set(pal, 20, 16, 70, 72), set(pal, 21, 16, 16, 16);
    memcpy(xxx, pal, sizeof xxx);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) body[y * W + x] = (uint8_t)(x < 8 ? 20 : ((x + y) & 1) ? 20 : 21);   /* solid, then a checkerboard */
    uint8_t before[W * H];
    memcpy(before, body, sizeof body);
    RepaintTexture tex[1] = { { body, W * H, REPAINT_MAIN, W, { { 0 } }, NULL } };
    const RepaintBank bank = { tex, 1, { pal, xxx }, 2, 0x24, 0x24 };
    const uint8_t blue[3] = { 30, 70, 200 };
    CHECK(car_repaint_paint(&bank, blue, 44) > 0);
    CHECK(body[3 * W + 3] != 20);                       /* the solid interior is painted */
    for (int y = 1; y < H - 1; y++)
        for (int x = 9; x < W - 1; x++) CHECK_EQ_INT(body[y * W + x], before[y * W + x]);   /* the checkerboard interior is as it was */
    CHECK_EQ_INT(pal[3 * 20 + 1], 70);                  /* and the entry it shows keeps its colour */
}

/* a near-black body's pairs are pulled towards the colour the main tone maps to, as the catalogue paints are: a PS1 body's bright
 * violet on a grey cover's highlights would streak */
static void test_pairs_soft(void)
{
    enum { N = 5, SIDE_W = 10 };
    static const uint8_t GREY[N] = { 24, 32, 40, 56, 72 };
    uint8_t pal[768] = { 0 }, xxx[768], body[N * 100];
    for (int i = 0; i < N; i++) set(pal, 10 + i, GREY[i], GREY[i], GREY[i]);
    memcpy(xxx, pal, sizeof xxx);
    for (int i = 0; i < N * 100; i++) body[i] = (uint8_t)(10 + i / 100);
    RepaintTexture tex[1] = { { body, N * 100, REPAINT_MAIN, 0, { { 0 } }, NULL } };
    const RepaintBank bank = { tex, 1, { pal, xxx }, 2, 0x24, 0x24 };
    const uint16_t g40 = (uint16_t)(5 | 5 << 5 | 5 << 10), g72 = (uint16_t)(9 | 9 << 5 | 9 << 10);
    const CarLiveryPair pairs[2] = { { g40, (uint16_t)(7 | 1 << 5 | 10 << 10) }, { g72, (uint16_t)(15 | 7 << 5 | 20 << 10) } };
    CHECK(car_repaint_pairs(&bank, pairs, 2, 20) > 0);
    CHECK_EQ_INT(pal[3 * 12], 56);                          /* the main tone (40) maps to its pair's colour */
    const int bright = pal[3 * 14];                         /* the 72 grey: 56 + (120 - 56) * 0.6 */
    CHECK(bright >= 90 && bright <= 98);
}

/* a lamp's light slot (entry 0xfe, which the game fills with the lamp's colour) and the lens and rim texels a few texels round it stay */
static void test_lamp_reach(void)
{
    enum { W = 24, H = 8 };
    uint8_t pal[768] = { 0 }, xxx[768], body[W * H];
    set(pal, 20, 120, 120, 124), set(pal, 21, 90, 90, 94);
    memcpy(xxx, pal, sizeof xxx);
    for (int i = 0; i < W * H; i++) body[i] = (uint8_t)(i % 2 ? 20 : 21);
    body[4 * W + 12] = 0xfe;
    uint8_t before[W * H];
    memcpy(before, body, sizeof body);
    RepaintTexture tex[1] = { { body, W * H, REPAINT_MAIN, W, { { 0 } }, NULL } };
    const RepaintBank bank = { tex, 1, { pal, xxx }, 2, 0x24, 0x24 };
    const uint8_t blue[3] = { 30, 70, 200 };
    CHECK(car_repaint_paint(&bank, blue, 44) > 0);
    CHECK_EQ_INT(body[4 * W + 12], 0xfe);
    for (int y = 3; y <= 5; y++)
        for (int x = 11; x <= 13; x++) CHECK(pal[3 * body[y * W + x] + 2] < 130);   /* the texel and the ring round it, as they were */
    CHECK(pal[3 * body[4 * W + 15] + 2] > 150);   /* three texels out: painted */
    CHECK(pal[3 * body[4 * W + 20] + 2] > 150);   /* far from it: painted */
    CHECK(pal[3 * body[4 * W + 2] + 2] > 150);
}

static void test_odd_banks(void)
{
    Fixture f;
    make(&f, 0x24);
    static uint8_t full[256];
    for (int i = 0; i < 256; i++) full[i] = (uint8_t)i;
    f.tex[2] = (RepaintTexture){ full, 256, REPAINT_NONE, 0, { 0 }, NULL };   /* every entry used: no spare */
    const uint8_t red[3] = { 200, 20, 20 };
    CHECK(car_repaint_paint(&f.bank, red, 44) >= 0);          /* nothing breaks */
    f.bank.type = 0x99;
    CHECK(car_repaint_paint(&f.bank, red, 44) < 0);           /* a palette of another type is left alone */
    CHECK(car_repaint_paint(NULL, red, 44) < 0);
}

/* a silver car's rear: the red and amber lamps stay byte for byte, the panel's silver is painted; on a red car the amber lamp and a
 * more saturated tail lamp stay too */
static void test_lamps(void)
{
    enum { SILVER = 30, SILVER2 = 31, LAMP_RED = 40, LAMP_AMBER = 41 };
    uint8_t pal[768] = { 0 }, xxx[768], rear[TEXELS], main_tex[TEXELS];
    set(pal, SILVER, 190, 190, 190), set(pal, SILVER2, 150, 150, 150), set(pal, LAMP_RED, 255, 20, 20), set(pal, LAMP_AMBER, 255, 160, 0);
    set(pal, 5, 16, 16, 16);
    memcpy(xxx, pal, sizeof xxx);
    for (int i = 0; i < TEXELS; i++) {
        main_tex[i] = (uint8_t)(i % 2 ? SILVER : SILVER2);
        rear[i] = (uint8_t)(i % 4 == 0 ? LAMP_RED : i % 4 == 1 ? LAMP_AMBER : i % 4 == 2 ? SILVER : 5);
    }
    uint8_t rear_before[TEXELS], pal_before[768];
    memcpy(rear_before, rear, sizeof rear), memcpy(pal_before, pal, sizeof pal);
    RepaintTexture tex[2] = { { main_tex, TEXELS, REPAINT_MAIN, 0, { 0 }, NULL }, { rear, TEXELS, REPAINT_END, 0, { 0 }, NULL } };
    const RepaintBank bank = { tex, 2, { pal, xxx }, 2, 0x24, 0x24 };
    const uint8_t blue[3] = { 30, 70, 200 };
    CHECK(car_repaint_paint(&bank, blue, 44) > 0);
    for (int i = 0; i < TEXELS; i++)
        if (rear_before[i] == LAMP_RED || rear_before[i] == LAMP_AMBER || rear_before[i] == 5) CHECK_EQ_INT(rear[i], rear_before[i]);
    CHECK_MEM(pal + 3 * LAMP_RED, pal_before + 3 * LAMP_RED, 3);
    CHECK_MEM(pal + 3 * LAMP_AMBER, pal_before + 3 * LAMP_AMBER, 3);
    int silver_texels = 0;
    for (int i = 0; i < TEXELS; i++)
        if (rear_before[i] == SILVER) silver_texels += pal[3 * rear[i] + 2] > pal[3 * rear[i]] + 40;
    CHECK_EQ_INT(silver_texels, TEXELS / 4);   /* the panel's own silver became the paint */

    uint8_t pal2[768] = { 0 }, xxx2[768], rear2[TEXELS], main2[TEXELS];
    set(pal2, 50, 150, 16, 16), set(pal2, 51, 110, 10, 10), set(pal2, 52, 70, 4, 4), set(pal2, LAMP_RED, 255, 20, 20);
    set(pal2, LAMP_AMBER, 255, 160, 0);
    memcpy(xxx2, pal2, sizeof xxx2);
    for (int i = 0; i < TEXELS; i++) {
        main2[i] = (uint8_t)(50 + i % 3);
        rear2[i] = (uint8_t)(i % 3 == 0 ? LAMP_RED : i % 3 == 1 ? LAMP_AMBER : 51);
    }
    memcpy(rear_before, rear2, sizeof rear2);
    RepaintTexture tex2[2] = { { main2, TEXELS, REPAINT_MAIN, 0, { 0 }, NULL }, { rear2, TEXELS, REPAINT_END, 0, { 0 }, NULL } };
    const RepaintBank red = { tex2, 2, { pal2, xxx2 }, 2, 0x24, 0x24 };
    const uint8_t white[3] = { 240, 240, 236 };
    CHECK(car_repaint_paint(&red, white, 44) > 0);
    for (int i = 0; i < TEXELS; i++)
        if (rear_before[i] == LAMP_RED || rear_before[i] == LAMP_AMBER) CHECK_EQ_INT(rear2[i], rear_before[i]);
    CHECK(pal2[3 * rear2[2] + 1] > 100);   /* the body's red in the rear panel became white */
}

/* a texture with a mask (1 keep, 0 paint) is repainted by the mask alone: a kept texel is as it was (its entry too), and every other
 * texel is painted, the glass under the cutoff and a lamp's slot included */
static void test_mask(void)
{
    Fixture f;
    make(&f, 0x24);
    f.tex[0].width = SIDE;
    f.body[21] = 0xfe;   /* a lamp slot: the rules would guard it; the mask says paint */
    set(f.pal, 0xfe, 100, 100, 104), set(f.xxx, 0xfe, 100, 100, 104), set(f.pal_before, 0xfe, 100, 100, 104);
    uint8_t mask[TEXELS], body_before[TEXELS];
    for (int i = 0; i < TEXELS; i++) mask[i] = (uint8_t)(i % 5 == 0);
    f.tex[0].mask = mask;
    memcpy(body_before, f.body, sizeof body_before);
    const uint8_t blue[3] = { 30, 70, 200 };
    CHECK(car_repaint_paint(&f.bank, blue, 44) > 0);
    for (int i = 0; i < TEXELS; i++) {
        if (mask[i]) {
            CHECK_EQ_INT(f.body[i], body_before[i]);
            CHECK_MEM(f.pal + 3 * f.body[i], f.pal_before + 3 * f.body[i], 3);
        } else {
            CHECK(colour(&f, f.pal, f.body[i], 2) > colour(&f, f.pal, f.body[i], 0) + 40);
        }
    }
}

/* the draft of a masked texture carries the mask's three classes: 0 paint, 128 keep with the shine (glass, lamps), 255 keep matte; a masked paint
 * texel among kept ones (and a mask 0 on a dithered or lamp-slot texel) is paint all the same, as the repaint does it */
static void test_draft_carries_the_masks_classes(void)
{
    Fixture f;
    make(&f, 0x24);
    f.tex[0].width = SIDE;
    f.body[7] = 0xfe;   /* a lamp slot, the mask says paint */
    uint8_t mask[TEXELS], keep0[TEXELS], keep1[TEXELS], keep2[TEXELS];
    for (int i = 0; i < TEXELS; i++) mask[i] = (uint8_t)(i % 3 == 0 ? 0 : i % 3 == 1 ? 128 : 255);
    mask[7] = 0, mask[6] = 255, mask[8] = 128;
    f.tex[0].mask = mask;
    uint8_t *const keeps[3] = { keep0, keep1, keep2 };
    CHECK(car_repaint_draft(&f.bank, 44, keeps) > 0);
    for (int i = 0; i < TEXELS; i++)
        CHECK_EQ_INT(keep0[i], mask[i] == 0 ? REPAINT_DRAFT_PAINT : mask[i] == 128 ? REPAINT_DRAFT_SHINE : REPAINT_DRAFT_MATTE);
}

/* the lamp classes (240 brake, 244 position, 248 headlight) shine by day exactly as the glass, class 128: a lamp texel is kept and drafted REPAINT_DRAFT_SHINE */
static void test_lamp_classes_shine_as_glass(void)
{
    Fixture f;
    make(&f, 0x24);
    f.tex[0].width = SIDE;
    uint8_t mask[TEXELS], keep0[TEXELS], keep1[TEXELS], keep2[TEXELS];
    for (int i = 0; i < TEXELS; i++) mask[i] = (uint8_t)(i == 0 ? 0 : i % 4 == 0 ? 128 : i % 4 == 1 ? 240 : i % 4 == 2 ? 244 : 248);
    f.tex[0].mask = mask;
    uint8_t *const keeps[3] = { keep0, keep1, keep2 };
    CHECK(car_repaint_draft(&f.bank, 44, keeps) > 0);
    CHECK_EQ_INT(keep0[0], REPAINT_DRAFT_PAINT);
    for (int i = 1; i < TEXELS; i++) CHECK_EQ_INT(keep0[i], REPAINT_DRAFT_SHINE);
}

/* a mask's 192 keeps the texel's colour (no recolour, as 128 and 255) and its draft class is the body's own, REPAINT_DRAFT_BODY */
static void test_a_body_class_texel_is_kept_and_drafted_as_body(void)
{
    Fixture f;
    make(&f, 0x24);
    f.tex[0].width = SIDE;
    uint8_t mask[TEXELS], keep0[TEXELS], keep1[TEXELS], keep2[TEXELS], body_before[TEXELS];
    for (int i = 0; i < TEXELS; i++) mask[i] = (uint8_t)(i % 4 == 0 ? 0 : i % 4 == 1 ? 128 : i % 4 == 2 ? 192 : 255);
    f.tex[0].mask = mask;
    memcpy(body_before, f.body, sizeof body_before);
    uint8_t *const keeps[3] = { keep0, keep1, keep2 };
    CHECK(car_repaint_draft(&f.bank, 44, keeps) > 0);
    for (int i = 0; i < TEXELS; i++)
        CHECK_EQ_INT(keep0[i], mask[i] == 0 ? REPAINT_DRAFT_PAINT : mask[i] == 128 ? REPAINT_DRAFT_SHINE : mask[i] == 192 ? REPAINT_DRAFT_BODY : REPAINT_DRAFT_MATTE);
    const uint8_t blue[3] = { 30, 70, 200 };
    CHECK(car_repaint_paint(&f.bank, blue, 44) > 0);
    for (int i = 0; i < TEXELS; i++)
        if (mask[i] == 192) CHECK_EQ_INT(f.body[i], body_before[i]);
}

/* a mask's 224 (fixed black): the texel takes the black paint's colour whatever paint the car gets (car_repaint_fixed_black, then the paint), is drafted as
 * paint (it shines as the body), and the paint leaves it alone; with no paint at all it is black too. The other classes keep their colours. */
static void test_a_fixed_black_texel_is_black_whatever_the_paint(void)
{
    Fixture f;
    make(&f, 0x24);
    f.tex[0].width = SIDE;
    uint8_t mask[TEXELS], keep0[TEXELS], keep1[TEXELS], keep2[TEXELS], body_before[TEXELS];
    for (int i = 0; i < TEXELS; i++) mask[i] = (uint8_t)(i % 4 == 0 ? 0 : i % 4 == 1 ? CAR_MASK_BLACK : i % 4 == 2 ? 128 : 255);
    f.tex[0].mask = mask;
    memcpy(body_before, f.body, sizeof body_before);
    uint8_t *const keeps[3] = { keep0, keep1, keep2 };
    CHECK(car_repaint_draft(&f.bank, 44, keeps) > 0);
    for (int i = 0; i < TEXELS; i++)
        if (mask[i] == CAR_MASK_BLACK) CHECK_EQ_INT(keep0[i], REPAINT_DRAFT_PAINT);
    const uint8_t black[3] = { 14, 14, 16 }, blue[3] = { 30, 70, 200 };
    CHECK(car_repaint_fixed_black(&f.bank, black, 44) > 0);
    for (int i = 0; i < TEXELS; i++) {
        const uint8_t *c = f.pal + 3 * f.body[i];
        if (mask[i] == CAR_MASK_BLACK) CHECK(c[0] <= 40 && c[1] <= 40 && c[2] <= 40);   /* black, not the 120 grey it was */
        else CHECK_EQ_INT(f.body[i], body_before[i]);   /* every other texel is as it was */
    }
    CHECK(car_repaint_paint(&f.bank, blue, 44) > 0);
    for (int i = 0; i < TEXELS; i++) {
        const uint8_t *c = f.pal + 3 * f.body[i];
        if (mask[i] == CAR_MASK_BLACK) CHECK(c[0] <= 40 && c[1] <= 40 && c[2] <= 40 && c[2] >= c[0]);   /* still black, under a blue paint */
        if (mask[i] == 0) CHECK(c[2] > c[0] + 20);   /* the paint's texels did go blue */
    }
}

/* a fixed black texel has the tone of the body under a black paint, not its own darker one: a 120 grey texel beside a 196 grey body comes out as the
 * body's black does (the rank of its lightness among the fixed texels, matched to the same rank among the body's pixels) */
static void test_a_fixed_black_texel_takes_the_bodys_tone(void)
{
    Fixture f, g;
    make(&f, 0x24), make(&g, 0x24);
    f.tex[0].width = g.tex[0].width = SIDE;
    uint8_t mask[TEXELS];
    for (int i = 0; i < TEXELS; i++) mask[i] = (uint8_t)(i % 4 == 0 ? 0 : i % 4 == 1 ? CAR_MASK_BLACK : i % 4 == 2 ? 128 : 255);
    f.tex[0].mask = mask, g.tex[0].mask = mask;
    const uint8_t black[3] = { 14, 14, 16 };
    CHECK(car_repaint_fixed_black(&f.bank, black, 44) > 0);
    CHECK(car_repaint_paint(&g.bank, black, 44) > 0);
    const uint8_t *body = g.pal + 3 * g.body[0];   /* a paint texel under the black paint */
    for (int i = 0; i < TEXELS; i++)
        if (mask[i] == CAR_MASK_BLACK)
            for (int c = 0; c < 3; c++) CHECK(abs((int)f.pal[3 * f.body[i] + c] - (int)body[c]) <= 2);
}

/* the PS1 pairs leave a colour far darker than the darkest pair; under a mask's paint it is mapped all the same */
static void test_mask_pairs(void)
{
    Fixture f;
    make(&f, 0x24);
    f.tex[0].width = SIDE;
    uint8_t mask[TEXELS];
    for (int i = 0; i < TEXELS; i++) mask[i] = (uint8_t)(i % 4 == 2);   /* the TRIM texels stay */
    f.tex[0].mask = mask;
    const uint16_t grey196 = (uint16_t)(24 | 24 << 5 | 24 << 10), grey120 = (uint16_t)(15 | 15 << 5 | 15 << 10);
    const CarLiveryPair pairs[2] = { { grey196, (uint16_t)(28 | 3 << 5 | 3 << 10) }, { grey120, (uint16_t)(14 | 1 << 5 | 1 << 10) } };
    CHECK(car_repaint_pairs(&f.bank, pairs, 2, 44) > 0);
    CHECK(colour(&f, f.pal, f.body[3], 0) > colour(&f, f.pal, f.body[3], 1) + 4);   /* the glass entry, 16 grey: reddish now */
    CHECK_MEM(f.pal + 3 * f.body[2], f.pal_before + 3 * TRIM, 3);
}

/* the draft of a mask: the texels the rules would not paint are 1 (matte) or 2 (shine) and nothing in the bank changes */
static void test_draft(void)
{
    Fixture f;
    make(&f, 0x24);
    f.tex[0].width = SIDE;
    f.tex[0].keep[0][0] = 0, f.tex[0].keep[0][1] = 0, f.tex[0].keep[0][2] = 2, f.tex[0].keep[0][3] = 1;   /* texels 0, 1 */
    f.body[27] = 0xfe;   /* a lamp slot: it and the ring round it shine; the TRIM texel 26 beside it would have been paint */
    f.body[44] = 0xfe;
    set(f.pal, 0xfe, 200, 40, 40), set(f.pal_before, 0xfe, 200, 40, 40);   /* a lamp's red: very saturated too */
    uint8_t body_before[TEXELS], keep0[TEXELS], keep1[TEXELS], keep2[TEXELS];
    memcpy(body_before, f.body, sizeof body_before);
    uint8_t *const keeps[3] = { keep0, keep1, keep2 };
    const int paint = car_repaint_draft(&f.bank, 44, keeps);
    CHECK(paint > 0);
    CHECK_MEM(f.body, body_before, TEXELS);
    CHECK_MEM(f.pal, f.pal_before, sizeof f.pal);
    CHECK_EQ_INT(keep0[0], REPAINT_DRAFT_MATTE);   /* in the kept rectangle: a plate */
    CHECK_EQ_INT(keep0[1], REPAINT_DRAFT_MATTE);
    CHECK_EQ_INT(keep0[2], REPAINT_DRAFT_PAINT);   /* TRIM: paint */
    CHECK_EQ_INT(keep0[3], REPAINT_DRAFT_SHINE);   /* the glass: a neutral under the floor */
    CHECK_EQ_INT(keep0[27], REPAINT_DRAFT_SHINE);
    CHECK_EQ_INT(keep0[26], REPAINT_DRAFT_SHINE);
    CHECK_EQ_INT(keep0[28], REPAINT_DRAFT_SHINE);
    CHECK_EQ_INT(keep0[44], REPAINT_DRAFT_SHINE);
    CHECK_EQ_INT(keep1[0], REPAINT_DRAFT_MATTE);   /* not a body texture: all kept */
}

/* the body textures by name and detail level: the high bank's are as before; the low bank's side and back (the far car's) are the
 * sides and the rear, as the high bank's rsid and rear are, so the far car repaints as the near one does */
static void test_body_class(void)
{
    CHECK_EQ_INT(car_repaint_body_class("topv", 0), REPAINT_MAIN);
    CHECK_EQ_INT(car_repaint_body_class("rsid", 0), REPAINT_MAIN);
    CHECK_EQ_INT(car_repaint_body_class("J001", 0), REPAINT_MAIN);
    CHECK_EQ_INT(car_repaint_body_class("frnt", 0), REPAINT_END);
    CHECK_EQ_INT(car_repaint_body_class("rear", 0), REPAINT_END);
    CHECK_EQ_INT(car_repaint_body_class("frnt", 1), REPAINT_END);
    CHECK_EQ_INT(car_repaint_body_class("rear", 1), REPAINT_END);
    CHECK_EQ_INT(car_repaint_body_class("side", 1), REPAINT_MAIN);
    CHECK_EQ_INT(car_repaint_body_class("back", 1), REPAINT_END);
    CHECK_EQ_INT(car_repaint_body_class("side", 0), REPAINT_NONE);   /* the high bank has no such texture: nothing changes there */
    CHECK_EQ_INT(car_repaint_body_class("back", 0), REPAINT_NONE);
    CHECK_EQ_INT(car_repaint_body_class("tyr1", 1), REPAINT_NONE);
    CHECK_EQ_INT(car_repaint_body_class("circ", 0), REPAINT_NONE);
}

/* a body texture's transparent index (0xff, skipped when drawn) is not paint: it stays 0xff, and its colour does not pull the family's hue */
static void test_transparent_index(void)
{
    Fixture f;
    make(&f, 0x24);
    for (int i = 0; i < TEXELS; i += 2) f.body[i] = 0xff;   /* half the body texture is cut out */
    set(f.pal, 0xff, 0, 0, 252);                             /* its palette colour is a saturated blue, as the low bank's */
    memcpy(f.xxx, f.pal, sizeof f.xxx);
    uint8_t cut[TEXELS];
    for (int i = 0; i < TEXELS; i++) cut[i] = f.body[i] == 0xff;
    const uint8_t red[3] = { 200, 30, 30 };
    CHECK(car_repaint_paint(&f.bank, red, 44) > 0);
    for (int i = 0; i < TEXELS; i++)
        if (cut[i]) CHECK_EQ_INT(f.body[i], 0xff);
    CHECK_EQ_INT(colour(&f, f.pal, 0xff, 2), 252);   /* and its colour is as it was */
}

int main(void)
{
    test_transparent_index();
    test_body_class();
    test_paint(0x24);
    test_paint(0x22);
    test_pairs();
    test_pairs_between();
    test_keep();
    test_pairs_dark();
    test_dither();
    test_pairs_soft();
    test_lamp_reach();
    test_odd_banks();
    test_lamps();
    test_mask();
    test_mask_pairs();
    test_draft();
    test_draft_carries_the_masks_classes();
    test_lamp_classes_shine_as_glass();
    test_a_body_class_texel_is_kept_and_drafted_as_body();
    test_a_fixed_black_texel_is_black_whatever_the_paint();
    test_a_fixed_black_texel_takes_the_bodys_tone();
    return TEST_REPORT();
}
