/* dep59 step 4b (T7): the CONTENT screen's preview reduction (game/frontend/content_preview.h): each pixel the mean of what it covers,
 * whole cells and cells only partly under it, and a single pixel the picture's mean. */
#include "game/frontend/content_preview.h"
#include "test.h"

#include <stdint.h>

/* a grey picture's pixel `i` set to `v` in all three channels, the green one inverted so the channels stay apart */
static void put(uint8_t *rgb, int i, int v)
{
    rgb[3 * i] = (uint8_t)v, rgb[3 * i + 1] = (uint8_t)(255 - v), rgb[3 * i + 2] = 7;
}

static void check_pixel(const uint8_t *out, int i, int v)
{
    CHECK_EQ_INT(out[3 * i], v);
    CHECK_EQ_INT(out[3 * i + 1], 255 - v);
    CHECK_EQ_INT(out[3 * i + 2], 7);
}

/* 4x4 to 2x2: each output pixel the mean of its own 2x2 block */
static void test_whole_blocks(void)
{
    static const int V[16] = { 0, 40, 100, 100, 80, 120, 100, 100, 200, 200, 10, 30, 200, 200, 50, 70 };
    uint8_t rgb[16 * 3], out[4 * 3];
    for (int i = 0; i < 16; i++) put(rgb, i, V[i]);
    content_reduce_area(rgb, 4, 4, out, 2, 2);
    check_pixel(out, 0, 60);
    check_pixel(out, 1, 100);
    check_pixel(out, 2, 200);
    check_pixel(out, 3, 40);
}

/* 3x3 to 2x2: each output pixel covers 1.5 x 1.5 cells, the middle row and column counted half and the centre a quarter */
static void test_fractional_cover(void)
{
    static const int V[9] = { 0, 90, 180, 90, 180, 0, 180, 0, 90 };
    uint8_t rgb[9 * 3], out[4 * 3];
    for (int i = 0; i < 9; i++) put(rgb, i, V[i]);
    content_reduce_area(rgb, 3, 3, out, 2, 2);
    check_pixel(out, 0, 60);    /* (0 + 90/2 + 90/2 + 180/4) / 2.25 */
    check_pixel(out, 1, 120);   /* (90/2 + 180 + 180/4 + 0/2) / 2.25 */
    check_pixel(out, 2, 120);
    check_pixel(out, 3, 60);
}

/* any picture to 1x1: the mean of every pixel */
static void test_one_pixel(void)
{
    static const int V[9] = { 0, 90, 180, 90, 180, 0, 180, 0, 90 };
    uint8_t rgb[9 * 3], out[3] = { 1, 2, 3 };
    for (int i = 0; i < 9; i++) put(rgb, i, V[i]);
    content_reduce_area(rgb, 3, 3, out, 1, 1);
    check_pixel(out, 0, 90);
}

/* a size of 0 writes nothing */
static void test_no_size(void)
{
    uint8_t rgb[3] = { 9, 9, 9 }, out[3] = { 1, 2, 3 };
    content_reduce_area(rgb, 1, 1, out, 0, 1);
    CHECK_EQ_INT(out[0], 1);
    CHECK_EQ_INT(out[2], 3);
}

int main(void)
{
    test_whole_blocks();
    test_fractional_cover();
    test_one_pixel();
    test_no_size();
    return TEST_REPORT();
}
