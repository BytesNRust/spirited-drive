/* The PS1 look's light script (psx_lgs.h) on made-up text in the real files' shape: CR line endings, spaced and comma
 * separated numbers, comments, a binary trailer. No game or CD data. */
#include "game/race/psx_lgs.h"
#include "game/sys/palette.h"
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

/* the night: the ambient colours (start, end and each context's) scaled per channel, rounded and kept in 0..255; the tint follows */
static void test_night_scale(void)
{
    static const char TEXT_CTX[] = "/* d */\n0,100, 10,95, 20,50, 100,50,\n/* s */\n128,100,60,\n/* e */\n128,100,60,\n";
    PsxLgs lgs;
    CHECK(psx_lgs_parse((const uint8_t *)TEXT_CTX, sizeof TEXT_CTX - 1, &lgs));
    lgs.context_count = 1;
    lgs.contexts[0] = (PsxLgsContext){ 0, 9, PSX_LGS_RAMP, { 200, 100, 50 }, { 255, 10, 0 } };
    const float mul[3] = { 0.5f, 0.25f, 2.0f };
    psx_lgs_scale(&lgs, mul, 1);
    CHECK_EQ_INT(lgs.start[0], 64);
    CHECK_EQ_INT(lgs.start[1], 25);
    CHECK_EQ_INT(lgs.start[2], 120);
    CHECK_EQ_INT(lgs.end[2], 120);
    CHECK_EQ_INT(lgs.contexts[0].c1[0], 100);
    CHECK_EQ_INT(lgs.contexts[0].c1[2], 100);
    CHECK_EQ_INT(lgs.contexts[0].c2[0], 128);   /* 127.5 rounded */
    CHECK_EQ_INT(lgs.contexts[0].c2[2], 0);
    const float big[3] = { 4, 4, 4 };
    psx_lgs_scale(&lgs, big, 1);
    CHECK_EQ_INT(lgs.start[2], 255);            /* kept in a byte */
    float tint[3];
    PsxLgs plain;
    CHECK(psx_lgs_parse((const uint8_t *)TEXT_CTX, sizeof TEXT_CTX - 1, &plain));
    const float one[3] = { 1, 1, 1 };
    psx_lgs_scale(&plain, one, 1);
    psx_lgs_tint(&plain, tint);
    CHECK(fabsf(tint[1] - 100.0f / 128.0f) < 1e-6f);   /* a multiplier of 1 changes nothing */
}

/* the night leaves the contexts (a tunnel's own lights) as the script has them: only the open road's colours (start and end) are scaled */
static void test_night_scale_keeps_the_contexts(void)
{
    static const char TEXT_CTX[] = "/* d */\n0,100, 10,95, 20,50, 100,50,\n/* s */\n128,100,60,\n/* e */\n128,100,60,\n";
    PsxLgs lgs;
    CHECK(psx_lgs_parse((const uint8_t *)TEXT_CTX, sizeof TEXT_CTX - 1, &lgs));
    lgs.context_count = 1;
    lgs.contexts[0] = (PsxLgsContext){ 0, 9, PSX_LGS_RAMP, { 200, 100, 50 }, { 255, 10, 0 } };
    const float mul[3] = { 0.5f, 0.5f, 0.5f };
    psx_lgs_scale(&lgs, mul, 0);
    CHECK_EQ_INT(lgs.start[0], 64);
    CHECK_EQ_INT(lgs.end[1], 50);
    CHECK_EQ_INT(lgs.contexts[0].c1[0], 200);
    CHECK_EQ_INT(lgs.contexts[0].c2[0], 255);
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

/* ZTR5 and ZTR6's scripts start with two ESC bytes before the first comment: the numbers are the same as without them */
static void test_leading_junk(void)
{
    static const char JUNK[] = "\x1b\x1b";
    char text[sizeof JUNK - 1 + sizeof TEXT];
    memcpy(text, JUNK, sizeof JUNK - 1);
    memcpy(text + sizeof JUNK - 1, TEXT, sizeof TEXT);
    PsxLgs plain, junk;
    CHECK(psx_lgs_parse((const uint8_t *)TEXT, sizeof TEXT - 1, &plain));
    CHECK(psx_lgs_parse((const uint8_t *)text, sizeof text - 1, &junk));
    CHECK(memcmp(plain.depth, junk.depth, sizeof plain.depth) == 0);
    CHECK(memcmp(plain.start, junk.start, sizeof plain.start) == 0 && memcmp(plain.end, junk.end, sizeof plain.end) == 0);
    CHECK_EQ_INT(junk.context_count, plain.context_count);
    CHECK_EQ_INT(junk.contexts[0].seg_ini, plain.contexts[0].seg_ini);
    CHECK_EQ_INT(junk.contexts[0].mode, plain.contexts[0].mode);
    static const char ONLY[] = "\x1b\x1b";
    CHECK(!psx_lgs_parse((const uint8_t *)ONLY, sizeof ONLY - 1, &junk));   /* nothing after them */
}

/* the contexts: one fixed (64), one ramp (96); outside them the default start -> end across the track, blended in and out */
/* a mode-96 context (kUseAC + kSwitchAC) covers its segments exactly: the tunnels' lamps switch the cars' light there */
static void test_switched(void)
{
    static const char T[] =
        "/* d */\r0,95 10,99 75,37 100,28\r/* s */\r100,100,100\r/* e */\r140,140,140,\r/* n */\r2\r"
        "10,12  64  60,80,100  0,0,0\r30,40  96  100,100,100  200,200,200\rCRCF";
    PsxLgs lgs;
    CHECK(psx_lgs_parse((const uint8_t *)T, sizeof T - 1, &lgs));
    CHECK(psx_lgs_switched(&lgs, 30) && psx_lgs_switched(&lgs, 35) && psx_lgs_switched(&lgs, 40));
    CHECK(!psx_lgs_switched(&lgs, 29) && !psx_lgs_switched(&lgs, 41));   /* no blend: the lamps start at the tunnel's mouth */
    CHECK(!psx_lgs_switched(&lgs, 11));                                  /* a fixed context is not switched */
    CHECK(!psx_lgs_switched(NULL, 35));
}

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

/* the ambient over the world's tint, to 1/step: exactly 1 where the ambient is the world's */
static void test_ratio(void)
{
    static const char T[] = "/* d */\r0,95 10,99 75,37 100,28\r/* s */\r100,100,100\r/* e */\r100,100,100,\r/* n */\r1\r"
                            "10,12  64  50,100,200  0,0,0\rCRCF";
    PsxLgs lgs;
    float r[3], tint[3] = { 100.0f / 128, 100.0f / 128, 100.0f / 128 };
    CHECK(psx_lgs_parse((const uint8_t *)T, sizeof T - 1, &lgs));
    psx_lgs_ratio(&lgs, 50, 100, tint, 16, r);
    CHECK(r[0] == 1.0f && r[1] == 1.0f && r[2] == 1.0f);
    psx_lgs_ratio(&lgs, 11, 100, tint, 16, r);          /* inside the context: 50/100, 100/100, 200/100, to 1/16 */
    CHECK(r[0] == 0.5f && r[1] == 1.0f && r[2] == 2.0f);
    psx_lgs_ratio(&lgs, 11, 100, tint, 4, r);
    CHECK(r[0] == 0.5f && r[2] == 2.0f);
    tint[0] = 0.0f;                                     /* a tint of nothing does not divide by it */
    psx_lgs_ratio(&lgs, 11, 100, tint, 16, r);
    CHECK(r[0] > 0.0f && r[0] <= 2.0f);
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

/* the exe's light of a point (T, the block's context, the node's parity) */
static void test_exe_light(void)
{
    CHECK_EQ_INT(psx_exe_t(0), 32);
    CHECK_EQ_INT(psx_exe_t(255), PSX_EXE_FULL);
    CHECK_EQ_INT(psx_exe_t(128), 96);
    CHECK_EQ_INT(psx_exe_t(510), 255);   /* clamped */
    CHECK_EQ_INT(psx_exe_t(-5), 32);
    PsxLgs lgs = { { 0 }, { 128, 128, 128 }, { 128, 128, 128 }, 3, { { 0 } } };
    lgs.contexts[0] = (PsxLgsContext){ 266, 292, PSX_LGS_RAMP, { 128, 115, 96 }, { 60, 60, 60 } };
    lgs.contexts[1] = (PsxLgsContext){ 125, 125, PSX_LGS_FIXED, { 128, 115, 96 }, { 0, 0, 0 } };
    lgs.contexts[2] = (PsxLgsContext){ 10, 10, PSX_LGS_FIXED, { 198, 198, 198 }, { 0, 0, 0 } };
    float f[3];
    psx_lgs_light(&lgs, 1200, 255, f);   /* open road: the LGT through T; the lit road is 1 */
    CHECK(f[0] == 1.0f && f[1] == 1.0f && f[2] == 1.0f);
    psx_lgs_light(&lgs, 1200, 0, f);     /* T's floor: 0.2, not black */
    CHECK(f[0] > 0.199f && f[0] < 0.201f);
    psx_lgs_light(NULL, 7, 128, f);      /* no script */
    CHECK(f[1] == 0.6f);
    psx_lgs_light(&lgs, 1100, 255, f);   /* in the tunnel (block 275): the context's colour, the LGT not read; even node: its first colour */
    CHECK(f[0] == 96.0f / 160 && f[2] == 80.0f / 160);
    psx_lgs_light(&lgs, 1101, 255, f);   /* odd node: its second colour (60: 32 + 30 = 62) */
    CHECK(f[0] == 62.0f / 160 && f[1] == f[0]);
    psx_lgs_light(&lgs, 1100, 0, f);     /* the same with another LGT byte */
    CHECK(f[0] == 96.0f / 160);
    psx_lgs_light(&lgs, 500, 255, f);    /* a FIXED context (block 125): the colour whatever the node's parity */
    psx_lgs_light(&lgs, 501, 255, f);
    CHECK(f[0] == 96.0f / 160 && f[2] == 80.0f / 160);
    psx_lgs_light(&lgs, 40, 255, f);     /* CY2's 198: 32 + 99 = 131, 0.82 (a multiplier of 1.56 it is not) */
    CHECK(f[0] == 131.0f / 160);
    psx_lgs_light(&lgs, 1063, 255, f);   /* the segment before the tunnel's first block (265): no blend at the edge */
    CHECK(f[0] == 1.0f);
}

/* a ring that is not a power of two (CY1's 1920) wraps by a modulo: a mask read node 1200 as 1072, inside the tunnel */
static void test_ring_and_car(void)
{
    CHECK_EQ_INT(psx_ring_node(1200, 1920), 1200);
    CHECK_EQ_INT(psx_ring_node(1920, 1920), 0);
    CHECK_EQ_INT(psx_ring_node(1921 + 1920, 1920), 1);
    CHECK_EQ_INT(psx_ring_node(-1, 1920), 1919);
    CHECK_EQ_INT(psx_ring_node(7, 0), 0);
    /* the track viewer's parked car stands `ahead` nodes on, wrapping the ring: node 1919 + 3 on CY1's 1920 is node 2; the mask it used gave 1922 & 1919 = 1794 */
    CHECK_EQ_INT(psx_ring_node(1919 + 3, 1920), 2);
    CHECK((uint32_t)((1919u + 3u) & (1920u - 1u)) != 2);
    PsxLgs lgs = { { 0 }, { 128, 128, 128 }, { 128, 128, 128 }, 1, { { 0 } } };
    lgs.contexts[0] = (PsxLgsContext){ 266, 292, PSX_LGS_RAMP, { 128, 115, 96 }, { 60, 60, 60 } };
    float r[3];
    psx_lgs_car_ratio(&lgs, 1920, 1200, 255, 16, r);   /* open road, segment 300 (masked: 1072, segment 268, the tunnel) */
    CHECK(r[0] == 1.0f && r[1] == 1.0f && r[2] == 1.0f);
    psx_lgs_car_ratio(&lgs, 1920, 1160, 255, 16, r);   /* the tunnel's last segments (290; masked 1032, no context): its first colour, even node */
    CHECK(r[0] == 0.625f && r[2] == 0.5f);
    psx_lgs_car_ratio(&lgs, 1920, 1161, 255, 16, r);   /* odd: the second, the lamps' alternation */
    CHECK(r[0] == 0.375f && r[2] == 0.375f);
    psx_lgs_car_ratio(&lgs, 1920, 1161 + 1920, 255, 16, r);   /* a lap on: the same place */
    CHECK(r[0] == 0.375f);
    psx_lgs_car_ratio(&lgs, 1920, 1200, 0, 16, r);     /* a dark LGT byte: 0.2 in sixteenths */
    CHECK(r[0] == 3.0f / 16);
}

/* a car's colours under a darker light stay in their hue family: a yellow does not go to olive, orange or pink where a palette has no darker
 * yellow nearer in RGB, and falls back to the nearest of all where the family has nothing */
static void test_lit_table_keeps_hue(void)
{
    uint8_t palette[768] = { 0 };
    const uint8_t colours[][3] = { { 240, 200, 0 }, { 150, 125, 0 }, { 150, 70, 20 }, { 120, 120, 120 }, { 60, 60, 60 } };
    for (int i = 0; i < 5; i++) memcpy(palette + 3 * i, colours[i], 3);
    for (int i = 5; i < 0xfe; i++) palette[3 * i] = palette[3 * i + 1] = palette[3 * i + 2] = 250;   /* far from everything above */
    uint8_t usable[256] = { 0 };
    memset(usable, 1, 0xfe);
    palette_usable_set(usable);
    const float ratio[3] = { 0.6f, 0.6f, 0.6f };
    uint8_t table[256];
    psx_lit_table(palette, ratio, table);
    CHECK_EQ_INT(table[0], 1);   /* the darker yellow, not the orange or the olive grey that are as near */
    CHECK_EQ_INT(table[3], 4);   /* a grey takes a grey */
}

/* the dash's art: the tint by `share` (half at dawn and dusk, the whole at night: the dash is dark with the world; only the dials keep their light) */
static void test_dash_palette_share(void)
{
    uint8_t palette[768] = { 0 }, out[768];
    palette[3] = 100, palette[4] = 200, palette[5] = 60;
    const float tint[3] = { 0.5f, 0.25f, 1.0f };
    psx_dash_palette(palette, tint, 1, 2, out);
    CHECK_EQ_INT(out[3], 75);
    CHECK_EQ_INT(out[4], 125);
    CHECK_EQ_INT(out[5], 60);   /* half way to the tinted */
    psx_dash_palette(palette, tint, 1, 1, out);
    CHECK_EQ_INT(out[3], 50);
    CHECK_EQ_INT(out[4], 50);
    CHECK_EQ_INT(out[5], 60);    /* the whole tint */
    CHECK_EQ_INT(out[0], 0);
}

int main(void)
{
    test_lit_table_keeps_hue();
    test_exe_light();
    test_ring_and_car();
    test_switched();
    test_mask_rect();
    test_palette_tint();
    test_world_mask();
    test_ambient_and_tint();
    test_night_scale();
    test_night_scale_keeps_the_contexts();
    test_neutral_and_rejected();
    test_leading_junk();
    test_ambient_contexts();
    test_lit_table();
    test_ratio();
    test_dash_palette_share();
    return TEST_REPORT();
}
