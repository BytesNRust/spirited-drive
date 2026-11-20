/* The pictures of a track's art (formats/fam_image.h): resampling keeps the average, the palette is 6-bit and holds the colours it was
 * made from, and no opaque pixel is given the transparent index 0xFF; a see-through one (alpha under 128) is, and gives the palette no colour. */
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

/* a see-through texel (alpha 0) next to an opaque red one: the half-size texel is red, half see-through, not a dark red */
static void test_resample_colour_by_alpha(void)
{
    uint8_t src[2 * 4] = { 255, 0, 0, 255, 0, 0, 0, 0 };
    FamPicture p = picture(src, 2, 1);
    uint8_t *out = fam_resample(&p, 1, 1);
    CHECK(out != NULL);
    if (out == NULL) return;
    CHECK_EQ_INT(out[0], 255);
    CHECK_EQ_INT(out[1], 0);
    CHECK_EQ_INT(out[3], 128);
    free(out);
}

/* alpha under 128 is the transparent index, and its colour is in no palette entry: a green that is never seen takes none of the 255 */
static void test_see_through_pixels(void)
{
    uint8_t rgba[4 * 4] = { 255, 0, 0, 255, 0, 255, 0, 0, 0, 255, 0, 127, 0, 0, 255, 128 };
    FamPicture p = picture(rgba, 4, 1);
    uint8_t pal[FAM_PALETTE_BYTES];
    CHECK(fam_palette_make(&p, 1, pal));
    uint8_t *idx = fam_indices(&p, pal);
    CHECK(idx != NULL);
    if (idx == NULL) return;
    CHECK(idx[0] != 0xff && idx[3] != 0xff);   /* opaque, and 128 is opaque enough */
    CHECK_EQ_INT(idx[1], 0xff);
    CHECK_EQ_INT(idx[2], 0xff);
    int green = 0;
    for (int i = 0; i < FAM_PIXEL_INDICES; i++) green += (pal[3 * i + 1] << 2) >= 128 && (pal[3 * i] << 2) < 128 && (pal[3 * i + 2] << 2) < 128;
    CHECK_EQ_INT(green, 0);
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

/* the shade the race draws under a car (0.6 of the colour, then the palette's nearest): of the generic road's lines the yellow and the white keep their hue and
 * land within AL1's own distance for its yellow (450); a palette cut for the pictures alone put the yellow's shade on a green */
static long shade_distance(const uint8_t pal[FAM_PALETTE_BYTES], uint8_t r, uint8_t g, uint8_t b, int *hue_r, int *hue_g, int *hue_b)
{
    const int c6[3] = { r >> 2, g >> 2, b >> 2 };
    int want[3];
    for (int i = 0; i < 3; i++) {
        const long product = (long)c6[i] * FAM_SHADE_SCALE;
        want[i] = (int)(product >> 16) + (int)((product >> 15) & 1);
    }
    long best = -1;
    for (int j = 0; j < FAM_PIXEL_INDICES; j++) {
        long d = 0;
        for (int i = 0; i < 3; i++) d += (long)((want[i] - pal[3 * j + i]) * 4) * ((want[i] - pal[3 * j + i]) * 4);
        if (best < 0 || d < best) best = d, *hue_r = pal[3 * j] << 2, *hue_g = pal[3 * j + 1] << 2, *hue_b = pal[3 * j + 2] << 2;
    }
    return best;
}

static void test_line_shades_keep_their_hue(void)
{
    /* a road like the generic one: asphalt greys with noise, a yellow and a white line, and grass and gravel */
    enum { W = 64, H = 64 };
    static uint8_t rgba[W * H * 4];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            uint8_t *px = rgba + 4 * (y * W + x);
            const int grey = 70 + ((x * 7 + y * 13) % 40);
            px[0] = (uint8_t)grey, px[1] = (uint8_t)(grey + 2), px[2] = (uint8_t)(grey + 4), px[3] = 255;
            if (y < 3) px[0] = 235, px[1] = 235, px[2] = 235;      /* the white edge line */
            if (y >= H - 3) px[0] = 240, px[1] = 190, px[2] = 0;   /* the yellow centre line */
            if (x > 55 && y > 20 && y < 40) px[0] = (uint8_t)(40 + y), px[1] = (uint8_t)(110 + x), px[2] = 30;   /* grass */
        }
    FamPicture p = { W, H, rgba };
    uint8_t pal[FAM_PALETTE_BYTES];
    CHECK(fam_palette_make(&p, 1, pal));
    int r, g, b;
    CHECK(shade_distance(pal, 240, 190, 0, &r, &g, &b) <= FAM_SHADE_BAR);   /* AL1's yellow shade is at 450 */
    CHECK(r > g && g > b + 40);                                              /* still a dark yellow, not the grass's green */
    CHECK(shade_distance(pal, 235, 235, 235, &r, &g, &b) <= FAM_SHADE_BAR);
    CHECK(abs(r - g) < 24 && abs(g - b) < 24);                               /* still a grey */
    CHECK_EQ_INT(fam_palette_index(pal, 240, 190, 0) != FAM_PIXEL_INDICES, 1);   /* the lines are still in the palette */
}

int main(void)
{
    test_resample_colour_by_alpha();
    test_see_through_pixels();
    test_resample_average();
    test_resample_up();
    test_palette_holds_the_colours();
    test_no_pixel_is_transparent();
    test_black_and_white_palette();
    test_line_shades_keep_their_hue();
    return TEST_REPORT();
}
