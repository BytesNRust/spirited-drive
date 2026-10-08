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

int main(void)
{
    test_scale();
    test_scale_range();
    test_fade_steps();
    return TEST_REPORT();
}
