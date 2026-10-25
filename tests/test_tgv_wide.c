/* port: a movie's picture scaled to a wide screen's width, cropped or barred to its height (tgv_wide.h) */
#include "game/frontend/tgv_wide.h"
#include "test.h"

#include <string.h>

/* no crop: a 4 x 2 picture at width 8 is 4 rows tall, every pixel twice across and down */
static void test_exact_double(void)
{
    const uint8_t src[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    uint8_t out[32];
    tgv_wide_scale(src, 4, 2, out, 8, 4);
    const uint8_t want[32] = { 1, 1, 2, 2, 3, 3, 4, 4, 1, 1, 2, 2, 3, 3, 4, 4,
                               5, 5, 6, 6, 7, 7, 8, 8, 5, 5, 6, 6, 7, 7, 8, 8 };
    CHECK(memcmp(out, want, sizeof out) == 0);
}

/* taller than the screen: the middle rows, the top and bottom cut (a 2 x 4 picture at width 4 is 8 rows, two a source row;
 * 4 are kept, 2 .. 5: the second and third source rows, twice each) */
static void test_crop_middle(void)
{
    const uint8_t src[8] = { 10, 11, 20, 21, 30, 31, 40, 41 };
    uint8_t out[16];
    tgv_wide_scale(src, 2, 4, out, 4, 4);
    const uint8_t want[16] = { 20, 20, 21, 21, 20, 20, 21, 21, 30, 30, 31, 31, 30, 30, 31, 31 };
    CHECK(memcmp(out, want, sizeof out) == 0);
}

/* shorter than the screen: black bars above and below, the picture between */
static void test_bars(void)
{
    const uint8_t src[4] = { 7, 8, 9, 6 };
    uint8_t out[32];
    memset(out, 0xaa, sizeof out);
    tgv_wide_scale(src, 4, 1, out, 8, 4);   /* 2 rows tall at width 8: one bar above, one below */
    const uint8_t want[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 8, 8, 9, 9, 6, 6,
                               7, 7, 8, 8, 9, 9, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0 };
    CHECK(memcmp(out, want, sizeof out) == 0);
}

/* a movie's own sizes at the wide formats: every output pixel is a pixel of the picture, none read outside it */
static void test_in_range(void)
{
    static uint8_t src[320 * 192], out[1280 * 480];
    for (int i = 0; i < 320 * 192; i++) src[i] = (uint8_t)(1 + i % 250);
    const int widths[] = { 854, 1120, 1280 };
    for (int k = 0; k < 3; k++) {
        tgv_wide_scale(src, 320, 192, out, widths[k], 480);
        for (int i = 0; i < widths[k] * 480; i++) CHECK(out[i] >= 1 && out[i] <= 250);   /* bars would be 0: none while the picture is taller than 480 */
    }
    /* the first and last columns and the middle row come from the picture's first and last columns */
    tgv_wide_scale(src, 320, 192, out, 854, 480);
    CHECK(out[240 * 854] == src[(size_t)(((2 * (240 + (512 - 480) / 2) + 1) * 192) / (2 * 512)) * 320]);
    CHECK(out[240 * 854 + 853] == src[(size_t)(((2 * (240 + (512 - 480) / 2) + 1) * 192) / (2 * 512)) * 320 + 319]);
}

/* the mode by the crop at the screen's width: 4:3 (no crop), 16:9 (32 of 480 rows, 6.7 %) fill; 21:9 (192 of 480, 40 %) fits */
static void test_mode(void)
{
    CHECK(tgv_wide_fills(320, 192, 640, 480));
    CHECK(tgv_wide_fills(320, 192, 854, 480));
    CHECK(!tgv_wide_fills(320, 192, 1280, 480));        /* 768 rows: 288 cut, 60 % */
    CHECK(!tgv_wide_fills(320, 192, 1120, 480));
    CHECK(!tgv_wide_fills(320, 192, 1008, 480));        /* 604 rows: 124 cut, 25.8 % */
    CHECK(tgv_wide_fills(320, 192, 880, 480));          /* 528 rows: 48 cut, exactly 10 % */
}

/* fitted: nothing cropped (the first and last picture rows are on the screen's), the middle is the picture at the
 * screen's height, the sides the enlarged frame through `shade`; filling is tgv_wide_scale unchanged */
static void test_fitted(void)
{
    static uint8_t src[320 * 192], out[1120 * 480], ref[1120 * 480], shade[256];
    for (int i = 0; i < 320 * 192; i++) src[i] = (uint8_t)(1 + i % 250);
    for (int c = 0; c < 256; c++) shade[c] = (uint8_t)(c / 2);
    tgv_wide_compose(src, 320, 192, out, 1120, 480, shade);
    CHECK(out[0 * 1120 + 560] == src[0 * 320 + 160]);                         /* the picture's top row is the screen's */
    CHECK(out[479 * 1120 + 560] == src[191 * 320 + 160]);
    const int left = (1120 - 800) / 2;
    CHECK(out[240 * 1120 + left] == src[96 * 320 + 0]);                        /* the picture's left edge, 800 wide */
    CHECK(out[240 * 1120 + left + 799] == src[96 * 320 + 319]);
    CHECK(out[240 * 1120] == shade[src[(size_t)((2 * ((672 - 480) / 2 + 240) + 1) * 192 / (2 * 672)) * 320]]);   /* a side: dimmed, enlarged */
    CHECK(out[240 * 1120 + 1119] == shade[src[(size_t)((2 * ((672 - 480) / 2 + 240) + 1) * 192 / (2 * 672)) * 320 + 319]]);
    CHECK(out[10 * 1120] == shade[src[(size_t)((2 * ((672 - 480) / 2 + 10) + 1) * 192 / (2 * 672)) * 320]]);
    /* the sides really are dimmed: no side pixel is a picture colour above the shade's range */
    for (int y = 0; y < 480; y++) for (int x = 0; x < left; x++) CHECK(out[y * 1120 + x] <= 125);
    /* filling: the same as tgv_wide_scale */
    tgv_wide_scale(src, 320, 192, ref, 854, 480);
    tgv_wide_compose(src, 320, 192, out, 854, 480, shade);
    CHECK(memcmp(out, ref, 854 * 480) == 0);
}

int main(void)
{
    test_mode();
    test_fitted();
    test_exact_double();
    test_crop_middle();
    test_bars();
    test_in_range();
    return TEST_REPORT();
}
