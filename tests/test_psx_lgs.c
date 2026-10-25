/* The PS1 look's light script (psx_lgs.h) on made-up text in the real files' shape: CR line endings, spaced and comma
 * separated numbers, comments, a binary trailer. No game or CD data. */
#include "game/race/psx_lgs.h"
#include "test.h"

#include <math.h>
#include <string.h>

static const char TEXT[] =
    "/* Depth ceuing definition X0,Y0, X1,Y1, X2,Y2, X3,Y3 */\r0,95  10,99  75,37  100,28\r"
    "/* Dynamic channel system (start rgb) */\r109,95,52\r/* Dynamic channel system (end rgb) */\r109,95,52,\r"
    "/* Number of light context */\r4\r/* context 1 */\r14,14  64  111,111,111  0,0,0\rCRCF\x01\x02\x03";

static void test_ambient_and_tint(void)
{
    PsxLgs lgs;
    CHECK(psx_lgs_parse((const uint8_t *)TEXT, sizeof TEXT - 1, &lgs));
    CHECK_EQ_INT(lgs.depth[0], 0);
    CHECK_EQ_INT(lgs.depth[7], 28);
    CHECK_EQ_INT(lgs.start[0], 109);
    CHECK_EQ_INT(lgs.start[2], 52);
    CHECK_EQ_INT(lgs.end[1], 95);
    float tint[3];
    psx_lgs_tint(&lgs, tint);
    CHECK(fabsf(tint[0] - 109.0f / 128.0f) < 1e-6f);
    CHECK(fabsf(tint[2] - 52.0f / 128.0f) < 1e-6f);
}

static void test_neutral_and_rejected(void)
{
    static const char NEUTRAL[] = "/* d */\n0,100, 10,95, 20,50, 100,50,\n/* s */\n128,128,128,\n/* e */\n128,128,128,\n";
    PsxLgs lgs;
    float tint[3];
    CHECK(psx_lgs_parse((const uint8_t *)NEUTRAL, sizeof NEUTRAL - 1, &lgs));
    psx_lgs_tint(&lgs, tint);
    CHECK(tint[0] == 1.0f && tint[1] == 1.0f && tint[2] == 1.0f);
    CHECK(!psx_lgs_parse((const uint8_t *)NEUTRAL, 20, &lgs));                      /* cut short */
    CHECK(!psx_lgs_parse((const uint8_t *)"/* nothing */ CRCF", 18, &lgs));        /* the trailer first */
    CHECK(!psx_lgs_parse(NULL, 10, &lgs));
    static const char BIG[] = "0,1,2,3,4,5,6,7,300,1,1,1,1,1";
    CHECK(!psx_lgs_parse((const uint8_t *)BIG, sizeof BIG - 1, &lgs));              /* a channel past 255 */
}

/* the contexts: one fixed (64), one ramp (96); outside them the default start -> end across the track, blended in and out */
static void test_ambient_contexts(void)
{
    static const char T[] =
        "/* d */\r0,95 10,99 75,37 100,28\r/* s */\r100,100,100\r/* e */\r140,140,140,\r/* n */\r2\r"
        "10,12  64  60,80,100  0,0,0\r30,40  96  100,100,100  200,200,200\rCRCF";
    PsxLgs lgs;
    float a[3];
    CHECK(psx_lgs_parse((const uint8_t *)T, sizeof T - 1, &lgs));
    CHECK_EQ_INT(lgs.context_count, 2);
    CHECK_EQ_INT(lgs.contexts[0].mode, 64);
    CHECK_EQ_INT(lgs.contexts[1].c2[2], 200);
    psx_lgs_ambient(&lgs, 11, 100, a);                      /* inside the fixed one */
    CHECK(fabsf(a[0] - 60.0f / 128) < 1e-5f && fabsf(a[2] - 100.0f / 128) < 1e-5f);
    psx_lgs_ambient(&lgs, 30, 100, a);                      /* the ramp's start */
    CHECK(fabsf(a[1] - 100.0f / 128) < 1e-5f);
    psx_lgs_ambient(&lgs, 35, 100, a);                      /* halfway: 150 */
    CHECK(fabsf(a[0] - 150.0f / 128) < 1e-5f);
    psx_lgs_ambient(&lgs, 40, 100, a);                      /* its end */
    CHECK(fabsf(a[0] - 200.0f / 128) < 1e-5f);
    psx_lgs_ambient(&lgs, 60, 100, a);                      /* default, 60% along: 100 + 0.6 * 40 = 124 */
    CHECK(fabsf(a[0] - 124.0f / 128) < 1e-5f);
    psx_lgs_ambient(&lgs, 9, 101, a);                       /* one segment before the fixed one: blended in, half way */
    const float def9 = (100.0f + 40.0f * 9 / 100) / 128, in9 = 60.0f / 128;
    CHECK(a[0] > in9 && a[0] < def9);
    psx_lgs_ambient(&lgs, 13, 101, a);                      /* and out again after it */
    CHECK(a[0] > 60.0f / 128 && a[0] < 113.0f / 128);
    psx_lgs_ambient(&lgs, 20, 0, a);                        /* no segment count: the start colour */
    CHECK(fabsf(a[0] - 100.0f / 128) < 1e-5f);
    PsxLgs none = lgs;
    none.context_count = 0;
    psx_lgs_ambient(&none, 11, 100, a);
    CHECK(fabsf(a[0] - (100.0f + 4.4f) / 128) < 1e-4f);
}

/* a car's table: each palette index to the entry nearest its colour times the ratio; shadow and see-through indices stay */
static void test_lit_table(void)
{
    uint8_t palette[768], table[256];
    for (int i = 0; i < 256; i++) palette[3 * i] = palette[3 * i + 1] = palette[3 * i + 2] = (uint8_t)i;
    const float same[3] = { 1, 1, 1 }, half[3] = { 0.5f, 0.5f, 0.5f }, warm[3] = { 1.0f, 0.8f, 0.4f };
    psx_lit_table(palette, same, table);
    for (int i = 0; i < 256; i++) CHECK_EQ_INT(table[i], i);
    psx_lit_table(palette, half, table);
    CHECK_EQ_INT(table[100], 50);
    CHECK_EQ_INT(table[200], 100);
    CHECK_EQ_INT(table[0xfe], 0xfe);
    CHECK_EQ_INT(table[0xff], 0xff);
    for (int i = 0; i < 256; i++) palette[3 * i + 1] = palette[3 * i + 2] = (uint8_t)(i / 2);   /* a red-ish ramp, then */
    psx_lit_table(palette, warm, table);
    CHECK(table[100] <= 100);                                  /* never brighter than it was at a ratio of 1 or less */
}

/* every palette entry is multiplied by the tint, each keeping its own colour (the 3D's pixels only are shown through it) */
static void test_palette_tint(void)
{
    uint8_t palette[768], tinted[768];
    for (int i = 0; i < 256; i++) palette[3 * i] = palette[3 * i + 1] = palette[3 * i + 2] = (uint8_t)(i / 2 + 60);   /* 128 steps */
    const float tint[3] = { 0.85f, 0.74f, 0.41f };
    psx_tint_palette(palette, tint, tinted);
    int distinct = 0, last = -1;
    for (int i = 0; i < 256; i++) {
        CHECK_EQ_INT(tinted[3 * i + 1], (int)(palette[3 * i + 1] * 0.74f + 0.5f));
        CHECK(tinted[3 * i + 2] < tinted[3 * i] && tinted[3 * i + 1] <= tinted[3 * i]);   /* warm */
        if (tinted[3 * i] != last) distinct++;
        last = tinted[3 * i];
    }
    CHECK(distinct > 100);   /* the texture's variation survives: a span over many indices keeps many colours */
    const float neutral[3] = { 1, 1, 1 };
    psx_tint_palette(palette, neutral, tinted);
    CHECK(memcmp(palette, tinted, sizeof palette) == 0);
    const float bright[3] = { 2.0f, 2.0f, 2.0f };
    psx_tint_palette(palette, bright, tinted);
    for (int i = 0; i < 768; i++) CHECK(tinted[i] >= palette[i]);   /* clamped, never wrapped */
}

/* a 3D pixel and a HUD pixel of the same index: the 3D one is the world's (it changes with the tint), the HUD's is not */
static void test_world_mask(void)
{
    enum { N = 6 };
    const uint8_t world[N] = { 100, 100, 7, 30, 100, 100 };   /* the 3D when it was done */
    uint8_t shown[N];
    memcpy(shown, world, N);
    shown[1] = 100 ^ 1;   /* the HUD wrote over pixel 1 ... */
    shown[3] = 100;       /* ... and over 3, with the same index the 3D's other pixels have */
    uint8_t mask[N];
    psx_world_mask(shown, world, N, mask);
    CHECK_EQ_INT(mask[0], 1);
    CHECK_EQ_INT(mask[1], 0);
    CHECK_EQ_INT(mask[2], 1);
    CHECK_EQ_INT(mask[3], 0);   /* index 100 as a HUD pixel stays; as pixels 0, 4, 5 it is tinted */
    CHECK_EQ_INT(mask[4], 1);
    uint8_t palette[768] = { 0 }, tinted[768];
    palette[3 * 100] = 200, palette[3 * 100 + 1] = 160, palette[3 * 100 + 2] = 100;
    const float tint[3] = { 0.5f, 0.5f, 0.5f };
    psx_tint_palette(palette, tint, tinted);
    const uint8_t *colour[N];
    for (int i = 0; i < N; i++) colour[i] = (mask[i] ? tinted : palette) + 3 * shown[i];
    CHECK_EQ_INT(colour[0][0], 100);   /* the world's index 100 darker and warmer */
    CHECK_EQ_INT(colour[3][0], 200);   /* the HUD's index 100 as it was */
}

/* the mirror's picture rectangle is the world's, clipped to the mask; nothing outside it changes */
static void test_mask_rect(void)
{
    uint8_t mask[8 * 6] = { 0 };
    psx_mask_rect(mask, 8, 6, 2, 1, 3, 2);
    for (int y = 0; y < 6; y++)
        for (int x = 0; x < 8; x++) CHECK_EQ_INT(mask[y * 8 + x], x >= 2 && x < 5 && y >= 1 && y < 3);
    psx_mask_rect(mask, 8, 6, 6, 4, 10, 10);   /* past the corner: clipped */
    CHECK_EQ_INT(mask[5 * 8 + 7], 1);
    CHECK_EQ_INT(mask[3 * 8 + 7], 0);
    psx_mask_rect(mask, 8, 6, -5, -5, 6, 6);   /* the top left corner only */
    CHECK_EQ_INT(mask[0], 1);
    CHECK_EQ_INT(mask[1 * 8 + 1], 0);
    uint8_t untouched[8 * 6] = { 0 };
    psx_mask_rect(untouched, 8, 6, 3, 3, 0, 4);   /* empty */
    for (int i = 0; i < 48; i++) CHECK_EQ_INT(untouched[i], 0);
}

int main(void)
{
    test_mask_rect();
    test_palette_tint();
    test_world_mask();
    test_ambient_and_tint();
    test_neutral_and_rejected();
    test_ambient_contexts();
    test_lit_table();
    return TEST_REPORT();
}
