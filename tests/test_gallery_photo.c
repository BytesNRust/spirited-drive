/* dep59 showcase (PR B): a photo cropped to fill a tile and the placeholder (game/frontend/gallery_photo.h). */
#include "game/frontend/gallery_photo.h"
#include "test.h"

#include <string.h>

static void test_cover(void)
{
    /* a 4x2 photo into 2x2: the factor 1 (the height covers), the middle 2 columns kept */
    const uint8_t src[4 * 2 * 3] = { 10, 0, 0, 20, 0, 0, 30, 0, 0, 40, 0, 0, 11, 0, 0, 21, 0, 0, 31, 0, 0, 41, 0, 0 };
    uint8_t dst[2 * 2 * 3];
    CHECK_EQ_INT(gallery_cover(src, 4, 2, dst, 2, 2), 1);
    CHECK(dst[0] == 20 && dst[3] == 30 && dst[6] == 21 && dst[9] == 31);
    /* a 2x4 photo into 2x2: the middle 2 rows */
    const uint8_t tall[2 * 4 * 3] = { 1, 0, 0, 2, 0, 0, 3, 0, 0, 4, 0, 0, 5, 0, 0, 6, 0, 0, 7, 0, 0, 8, 0, 0 };
    CHECK_EQ_INT(gallery_cover(tall, 2, 4, dst, 2, 2), 1);
    CHECK(dst[0] == 3 && dst[3] == 4 && dst[6] == 5 && dst[9] == 6);
    /* shrunk: a 4x4 of two flat halves into 2x2 averages each box; a flat photo stays flat */
    uint8_t flat[4 * 4 * 3], out[2 * 2 * 3];
    memset(flat, 90, sizeof flat);
    CHECK_EQ_INT(gallery_cover(flat, 4, 4, out, 2, 2), 1);
    for (size_t i = 0; i < sizeof out; i++) CHECK_EQ_INT(out[i], 90);
    /* the average of a box: 0 and 100 in a 2x1 box is 50 */
    const uint8_t pair[2 * 1 * 3] = { 0, 0, 0, 100, 100, 100 };
    uint8_t one[3];
    CHECK_EQ_INT(gallery_cover(pair, 2, 1, one, 1, 1), 1);
    CHECK(one[0] == 50 && one[1] == 50 && one[2] == 50);
    /* enlarged: a 1x1 photo fills 3x2 */
    const uint8_t dot[3] = { 7, 8, 9 };
    uint8_t big[3 * 2 * 3];
    CHECK_EQ_INT(gallery_cover(dot, 1, 1, big, 3, 2), 1);
    for (int i = 0; i < 6; i++) CHECK(big[3 * i] == 7 && big[3 * i + 1] == 8 && big[3 * i + 2] == 9);
    CHECK_EQ_INT(gallery_cover(dot, 0, 1, big, 3, 2), 0);   /* a size that is not one */
    CHECK_EQ_INT(gallery_cover(dot, 1, 1, big, 3, 0), 0);
}

static void test_placeholder(void)
{
    static uint8_t ground[30 * 20 * 3], out[30 * 20 * 3];
    memset(ground, 200, sizeof ground);
    CHECK_EQ_INT(gallery_placeholder(ground, 30, 20, out, 30, 20), 1);
    CHECK_EQ_INT(out[3 * (10 * 30 + 15)], 80);      /* darkened: 2/5 of the ground */
    CHECK_EQ_INT(out[3 * (6 * 30 + 15)], 190);      /* the line inside: lighter than the rest, 3/4 + 40 */
    CHECK_EQ_INT(out[0], 80);                       /* the margin outside it as the rest */
    CHECK_EQ_INT(gallery_placeholder(ground, 10, 10, out, 30, 20), 0);   /* a ground smaller than the tile */
}

int main(void)
{
    test_cover();
    test_placeholder();
    return TEST_REPORT();
}
