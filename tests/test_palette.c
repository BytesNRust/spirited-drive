/* Palette helpers used by the fades. Scaling follows 004920bc:
 * dst = src * step >> 5 over count bytes starting at colour `first`. */
#include "game/sys/palette.h"
#include "test.h"

static void test_scale(void)
{
    uint8_t src[768], dst[768];
    for (int i = 0; i < 768; i++) src[i] = (uint8_t)(i & 0xFF);
    memset(dst, 0xAA, sizeof dst);

    palette_scale(src, dst, 0, 256, 32);   /* full brightness */
    CHECK_MEM(dst, src, 768);

    palette_scale(src, dst, 0, 256, 0);    /* black */
    CHECK_EQ_INT(dst[255], 0);

    palette_scale(src, dst, 0, 256, 16);   /* half: 255 * 16 >> 5 = 127 */
    CHECK_EQ_INT(dst[255], 127);
    CHECK_EQ_INT(dst[3], 1);
}

static void test_scale_range(void)
{
    uint8_t src[768], dst[768];
    memset(src, 200, sizeof src);
    memset(dst, 7, sizeof dst);

    /* Only colours 10..11 are written, read from the same colours in src,
     * into the start of dst (004920bc packs the result from dst[0]). */
    palette_scale(src, dst, 10, 2, 8);     /* 200 * 8 >> 5 = 50 */
    CHECK_EQ_INT(dst[0], 50);
    CHECK_EQ_INT(dst[5], 50);
    CHECK_EQ_INT(dst[6], 7);
}

static void test_fade_steps(void)
{
    /* 00463e7c: steps 0, 2, ..., 32 (17 frames); 00463ef0: 32, 30, ..., 0. */
    CHECK_EQ_INT(PALETTE_FADE_STEPS, 17);
    CHECK_EQ_INT(palette_fade_in_step(0), 0);
    CHECK_EQ_INT(palette_fade_in_step(16), 32);
    CHECK_EQ_INT(palette_fade_out_step(0), 32);
    CHECK_EQ_INT(palette_fade_out_step(16), 0);
}

static void test_nearest(void)
{
    uint8_t pal[768] = { 0 };
    uint8_t usable[256];
    memset(usable, 1, sizeof usable);
    pal[3 * 1] = 250; pal[3 * 1 + 1] = 10;  pal[3 * 1 + 2] = 10;   /* red */
    pal[3 * 2] = 10;  pal[3 * 2 + 1] = 250; pal[3 * 2 + 2] = 10;   /* green */
    pal[3 * 3] = 250; pal[3 * 3 + 1] = 10;  pal[3 * 3 + 2] = 10;   /* red again */

    CHECK_EQ_INT(palette_nearest(pal, usable, 255, 0, 0), 1);      /* first of equals wins */
    CHECK_EQ_INT(palette_nearest(pal, usable, 0, 200, 0), 2);
    usable[1] = 0;
    CHECK_EQ_INT(palette_nearest(pal, usable, 255, 0, 0), 3);      /* unusable entries skipped */
    CHECK_EQ_INT(palette_nearest(pal, usable, 0, 0, 0), 0);
}

/* 0048f370: level i = (fg * i + bg * (15 - i)) >> 4 per channel, then nearest. */
static void test_text_ramp(void)
{
    uint8_t pal[768] = { 0 };
    uint8_t usable[256];
    uint8_t ramp[16];
    memset(usable, 1, sizeof usable);
    usable[254] = usable[255] = 0;
    for (int i = 0; i < 256; i++) pal[3 * i] = pal[3 * i + 1] = pal[3 * i + 2] = (uint8_t)i;  /* grey ramp */
    pal[3 * 255] = pal[3 * 255 + 1] = pal[3 * 255 + 2] = 240;  /* unusable, closer for level 15 */

    palette_text_ramp(pal, usable, 200, 0, ramp);  /* fg = grey 200, bg = black */
    CHECK_EQ_INT(ramp[0], 0);
    CHECK_EQ_INT(ramp[8], (200 * 8) >> 4);    /* 100 */
    CHECK_EQ_INT(ramp[15], (200 * 15) >> 4);  /* 187: never full fg, divides by 16 */
}

/* 004824a0 via 0048263c(k, k, k): channel * k / 65536 rounded half up, clamped
 * to 63, then the colour with the smallest 2*dr^2 + 3*dg^2 + db^2 (< 9999). */
static void test_shade_table(void)
{
    uint8_t pal[768] = { 0 };
    uint8_t usable[256], table[256];
    memset(usable, 1, sizeof usable);
    for (int i = 0; i < 256; i++) pal[3 * i] = pal[3 * i + 1] = pal[3 * i + 2] = (uint8_t)i;

    palette_shade_table(pal, usable, 0x9000, table);
    CHECK_EQ_INT(table[0], 0);
    CHECK_EQ_INT(table[100], 56);   /* 100 * 0.5625 = 56.25 -> 56 */
    CHECK_EQ_INT(table[104], 59);   /* 58.5 rounds up */
    CHECK_EQ_INT(table[200], 63);   /* 112.5 clamped to 63 */
    CHECK_EQ_INT(table[255], 63);
}

int main(void)
{
    test_shade_table();
    test_nearest();
    test_text_ramp();
    test_scale();
    test_scale_range();
    test_fade_steps();
    return TEST_REPORT();
}
