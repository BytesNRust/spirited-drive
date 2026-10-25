/* The texel-scoped repaint of a car's bank (car_repaint.h) on a synthetic bank: the body textures change, nothing else does. */
#include "game/race/car_repaint.h"
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
    f->tex[0] = (RepaintTexture){ f->body, TEXELS, REPAINT_MAIN };
    f->tex[1] = (RepaintTexture){ f->tyre, TEXELS, REPAINT_NONE };
    f->tex[2] = (RepaintTexture){ f->decal, TEXELS, REPAINT_NONE };
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
    CHECK(colour(&f, f.pal, f.body[2], 0) > colour(&f, f.pal, f.body[2], 1) + 40);    /* the unpaired grey red too */
    CHECK_EQ_INT(f.body[3], GLASS);
}

static void test_odd_banks(void)
{
    Fixture f;
    make(&f, 0x24);
    static uint8_t full[256];
    for (int i = 0; i < 256; i++) full[i] = (uint8_t)i;
    f.tex[2] = (RepaintTexture){ full, 256, REPAINT_NONE };   /* every entry used: no spare */
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
    RepaintTexture tex[2] = { { main_tex, TEXELS, REPAINT_MAIN }, { rear, TEXELS, REPAINT_END } };
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
    RepaintTexture tex2[2] = { { main2, TEXELS, REPAINT_MAIN }, { rear2, TEXELS, REPAINT_END } };
    const RepaintBank red = { tex2, 2, { pal2, xxx2 }, 2, 0x24, 0x24 };
    const uint8_t white[3] = { 240, 240, 236 };
    CHECK(car_repaint_paint(&red, white, 44) > 0);
    for (int i = 0; i < TEXELS; i++)
        if (rear_before[i] == LAMP_RED || rear_before[i] == LAMP_AMBER) CHECK_EQ_INT(rear2[i], rear_before[i]);
    CHECK(pal2[3 * rear2[2] + 1] > 100);   /* the body's red in the rear panel became white */
}

int main(void)
{
    test_paint(0x24);
    test_paint(0x22);
    test_pairs();
    test_odd_banks();
    test_lamps();
    return TEST_REPORT();
}
