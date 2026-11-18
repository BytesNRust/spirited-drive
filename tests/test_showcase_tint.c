/* dep59 step 5 (T7d): the one function that tints a showcase page's background, for a track's page (mod.ini showcase_tint=#RRGGBB) and
 * a car's (its main colour): the key's parse, and the tint keeping each entry's brightness. */
#include "formats/showcase_tint.h"
#include "test.h"

#include <stdint.h>
#include <string.h>

static int luma(const uint8_t *p) { return (299 * p[0] + 587 * p[1] + 114 * p[2]) / 1000; }

static void test_the_key_parses(void)
{
    uint8_t rgb[3] = { 9, 9, 9 };
    CHECK_EQ_INT(showcase_tint_parse("#1A2b3C", rgb), 1);
    CHECK(rgb[0] == 0x1a && rgb[1] == 0x2b && rgb[2] == 0x3c);
    CHECK_EQ_INT(showcase_tint_parse("#000000", rgb), 1);
    const char *bad[] = { "", "#", "123456", "#12345", "#1234567", "#12345g", "# 12345", " #123456", "#123456 ", NULL };
    for (int i = 0; bad[i] != NULL; i++) {
        rgb[0] = rgb[1] = rgb[2] = 7;
        CHECK_EQ_INT(showcase_tint_parse(bad[i], rgb), 0);
        CHECK(rgb[0] == 7 && rgb[1] == 7 && rgb[2] == 7);   /* untouched on a refusal */
    }
    CHECK_EQ_INT(showcase_tint_parse(NULL, rgb), 0);
}

static void test_the_tint_keeps_the_brightness(void)
{
    const uint8_t tint[3] = { 0x70, 0x42, 0x14 };
    uint8_t px[4 * 3] = { 0, 0, 0, 40, 40, 40, 120, 130, 110, 200, 200, 200 };
    uint8_t before[sizeof px];
    memcpy(before, px, sizeof px);
    showcase_tint_apply(px, 3, tint);   /* three entries: the fourth is left alone */
    CHECK_MEM(px + 9, before + 9, 3);
    CHECK_EQ_INT(px[0] + px[1] + px[2], 0);   /* black stays black */
    for (int i = 1; i < 3; i++) CHECK(luma(px + 3 * i) >= luma(before + 3 * i) - 3 && luma(px + 3 * i) <= luma(before + 3 * i) + 3);
    CHECK(px[3] > px[5] && px[6] > px[8]);   /* warm: red over blue, as the tint is */
}

static void test_the_tint_is_deterministic_and_total(void)
{
    const uint8_t tint[3] = { 0xff, 0xff, 0xff }, dark[3] = { 0, 0, 0 };
    uint8_t a[6] = { 255, 255, 255, 10, 200, 30 }, b[6];
    memcpy(b, a, sizeof a);
    showcase_tint_apply(a, 2, tint);
    showcase_tint_apply(b, 2, tint);
    CHECK_MEM(a, b, sizeof a);
    CHECK(a[0] == 255 && a[1] == 255 && a[2] == 255);   /* the brightest stays in range */
    uint8_t c[3] = { 100, 150, 50 };
    showcase_tint_apply(c, 1, dark);   /* a black tint: grey of the same brightness, no division by zero */
    CHECK(c[0] == c[1] && c[1] == c[2]);
    CHECK_EQ_INT(c[0], luma((const uint8_t[]){ 100, 150, 50 }));
    showcase_tint_apply(NULL, 5, tint);   /* nothing to do, no fault */
    showcase_tint_apply(c, 0, tint);
}

static void test_the_default_is_a_sepia(void)
{
    CHECK(SHOWCASE_TINT_SEPIA[0] > SHOWCASE_TINT_SEPIA[1] && SHOWCASE_TINT_SEPIA[1] > SHOWCASE_TINT_SEPIA[2]);
}

int main(void)
{
    test_the_key_parses();
    test_the_tint_keeps_the_brightness();
    test_the_tint_is_deterministic_and_total();
    test_the_default_is_a_sepia();
    return TEST_REPORT();
}
