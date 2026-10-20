/* The menus' darkened, enlarged backdrop on a wide screen (port glue, no original counterpart). */
#include "game/sys/backdrop.h"
#include "test.h"

#include <stdlib.h>

enum { W = 640, H = 480 };

static void grey_palette(uint8_t rgb[768])
{
    for (int i = 0; i < 256; i++) rgb[3 * i] = rgb[3 * i + 1] = rgb[3 * i + 2] = (uint8_t)i;
}

static void test_shade_table(void)
{
    uint8_t rgb[768], shade[256];
    grey_palette(rgb);
    backdrop_shade_table(rgb, shade);
    CHECK_EQ_INT(shade[0], 0);
    CHECK_EQ_INT(shade[255], 115);  /* 45 % of 255 = 114.75 */
    CHECK_EQ_INT(shade[100], 45);
    CHECK_EQ_INT(shade[200], 90);

    /* a colour palette: red stays red-ish, never brighter than the colour itself */
    for (int i = 0; i < 256; i++) rgb[3 * i] = (uint8_t)i, rgb[3 * i + 1] = 0, rgb[3 * i + 2] = 0;
    backdrop_shade_table(rgb, shade);
    CHECK_EQ_INT(shade[200], 90);
    CHECK_EQ_INT(shade[255], 115);
}

static void test_compose(void)
{
    uint8_t *src = malloc((size_t)W * H);
    uint8_t shade[256];
    for (size_t i = 0; i < (size_t)W * H; i++) src[i] = (uint8_t)(i * 7u + 3u);
    for (int c = 0; c < 256; c++) shade[c] = (uint8_t)(255 - c);

    uint8_t *same = malloc((size_t)W * H);
    backdrop_compose(src, same, W, shade);
    CHECK_MEM(same, src, (size_t)W * H);
    free(same);

    const int dst_w = 854, left = 107;
    uint8_t *dst = malloc((size_t)dst_w * H);
    backdrop_compose(src, dst, dst_w, shade);
    int centre_equal = 1;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) centre_equal &= dst[y * dst_w + left + x] == src[y * W + x];
    CHECK(centre_equal);
    CHECK_EQ_INT(dst[240 * dst_w + 0], shade[src[240 * W + 0]]);
    CHECK_EQ_INT(dst[240 * dst_w + (dst_w - 1)], shade[src[240 * W + 639]]);
    CHECK_EQ_INT(dst[0], shade[src[60 * W + 0]]);                       /* the top 80 scaled lines are cut */
    CHECK_EQ_INT(dst[479 * dst_w + 5], shade[src[419 * W + 4]]);        /* x 5 -> 4, y 479 + 80 -> 419 */
    CHECK_EQ_INT(dst[100 * dst_w + 50], shade[src[((2 * (100 + 80) + 1) * 640 / (2 * dst_w)) * W
                                                  + (2 * 50 + 1) * 640 / (2 * dst_w)]]);
    free(dst);
}

int main(void)
{
    test_shade_table();
    test_compose();
    return TEST_REPORT();
}
