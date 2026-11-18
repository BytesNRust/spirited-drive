/* The pictures of a track's art (formats/fam_image.h): resampling keeps the average, the palette is 6-bit and holds the colours it was
 * made from, and no pixel is given the transparent index 0xFF. */
#include "formats/fam_image.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static FamPicture picture(uint8_t *rgba, uint32_t w, uint32_t h) { return (FamPicture){ w, h, rgba }; }

static void test_resample_average(void)
{
    uint8_t src[4 * 4 * 4];
    for (int i = 0; i < 16; i++) src[4 * i] = (uint8_t)(i < 8 ? 0 : 200), src[4 * i + 1] = 0, src[4 * i + 2] = 0, src[4 * i + 3] = 255;
    FamPicture p = picture(src, 4, 4);
    uint8_t *out = fam_resample(&p, 2, 2);
    CHECK(out != NULL);
    if (out == NULL) return;
    CHECK_EQ_INT(out[0], 0);       /* the top half is black: the top row of the 2 x 2 */
    CHECK_EQ_INT(out[4], 0);
    CHECK_EQ_INT(out[8], 200);     /* the bottom half is 200 */
    CHECK_EQ_INT(out[12], 200);
    CHECK_EQ_INT(out[3], 255);
    free(out);
}

static void test_resample_up(void)
{
    uint8_t src[4] = { 10, 20, 30, 255 };
    FamPicture p = picture(src, 1, 1);
    uint8_t *out = fam_resample(&p, 4, 3);
    CHECK(out != NULL);
    if (out == NULL) return;
    CHECK_EQ_INT(out[(2 * 4 + 3) * 4], 10);   /* every texel of a 1 x 1 is that texel */
    CHECK_EQ_INT(out[(2 * 4 + 3) * 4 + 2], 30);
    free(out);
}

static void test_palette_holds_the_colours(void)
{
    uint8_t rgba[2 * 4] = { 255, 0, 0, 255, 0, 0, 255, 255 };
    FamPicture p = picture(rgba, 2, 1);
    uint8_t pal[FAM_PALETTE_BYTES];
    CHECK(fam_palette_make(&p, 1, pal));
    const uint8_t red = fam_palette_index(pal, 255, 0, 0), blue = fam_palette_index(pal, 0, 0, 255);
    CHECK(red != FAM_PIXEL_INDICES && blue != FAM_PIXEL_INDICES);
    CHECK(red != blue);
    CHECK_EQ_INT((pal[3 * red] << 2) >= 248, 1);   /* 6-bit: the red reads back as 252 */
    CHECK_EQ_INT(pal[3 * red + 2] << 2, 0);
    CHECK_EQ_INT((pal[3 * blue + 2] << 2) >= 248, 1);
}

static void test_no_pixel_is_transparent(void)
{
    /* 256 greys, and a second picture of other colours: more colours than the palette holds, so the median cut has to merge some */
    enum { SIDE = 16, COUNT = SIDE * SIDE * 2 };
    uint8_t rgba[COUNT * 4];
    for (int i = 0; i < COUNT; i++) {
        const uint8_t v = (uint8_t)(i * 37);
        rgba[4 * i] = v, rgba[4 * i + 1] = (uint8_t)(v ^ 0x55), rgba[4 * i + 2] = (uint8_t)(255 - v), rgba[4 * i + 3] = 255;
    }
    FamPicture p = picture(rgba, SIDE * 2, SIDE);
    uint8_t pal[FAM_PALETTE_BYTES];
    CHECK(fam_palette_make(&p, 1, pal));
    uint8_t *idx = fam_indices(&p, pal);
    CHECK(idx != NULL);
    if (idx == NULL) return;
    int transparent = 0;
    for (int i = 0; i < COUNT; i++) transparent += idx[i] == 0xff;
    CHECK_EQ_INT(transparent, 0);
    free(idx);
}

static void test_black_and_white_palette(void)
{
    uint8_t rgba[4 * 3] = { 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
    FamPicture p = picture(rgba, 3, 1);
    uint8_t pal[FAM_PALETTE_BYTES];
    CHECK(fam_palette_make(&p, 1, pal));
    CHECK_EQ_INT(fam_palette_index(pal, 0, 0, 0) == fam_palette_index(pal, 3, 3, 3), 1);
    CHECK_EQ_INT(fam_palette_index(pal, 255, 255, 255) == fam_palette_index(pal, 0, 0, 0), 0);
}

int main(void)
{
    test_resample_average();
    test_resample_up();
    test_palette_holds_the_colours();
    test_no_pixel_is_transparent();
    test_black_and_white_palette();
    return TEST_REPORT();
}
