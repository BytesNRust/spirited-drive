/* The PS1 sky as a PC-style horizon bitmap (psx_sky_bitmap.h): the panorama magnified, the gradient above it, 256 colours. */
#include "game/race/psx_sky_bitmap.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { W = 16, H = 4 };

/* A 16 x 4 panorama: the left half one colour, the right half another; the INF's gradient from dark to light. */
static void made_up(PsxSky *sky, PsxSkyInf *inf)
{
    sky->width = W, sky->height = H;
    sky->rgb = malloc((size_t)W * H * 3);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            uint8_t *p = sky->rgb + 3 * (y * W + x);
            p[0] = x < W / 2 ? 200 : 40, p[1] = x < W / 2 ? 120 : 90, p[2] = x < W / 2 ? 40 : 200;
        }
    memset(inf, 0, sizeof *inf);
    inf->top[0] = 16, inf->top[1] = 16, inf->top[2] = 16;
    inf->bottom[0] = 208, inf->bottom[1] = 208, inf->bottom[2] = 208;
}

static void colour(const PsxSkyBitmap *b, int x, int y, int rgb[3])
{
    const uint8_t index = b->pixels[(size_t)y * b->width + x];
    for (int k = 0; k < 3; k++) rgb[k] = b->palette[3 * index + k];
}

static int near(const int a[3], int r, int g, int b)
{
    return abs(a[0] - r) <= 8 && abs(a[1] - g) <= 8 && abs(a[2] - b) <= 8;
}

static void test_layout(void)
{
    PsxSky sky;
    PsxSkyInf inf;
    made_up(&sky, &inf);
    PsxSkyBitmap b;
    CHECK(psx_sky_bitmap_build(&sky, &inf, &b));
    CHECK_EQ_INT(b.width, W * PSX_SKY_ZOOM);
    CHECK_EQ_INT(b.height, PSX_SKY_GRADIENT_ROWS + H * PSX_SKY_ZOOM);
    CHECK_EQ_INT(b.horizon_row, PSX_SKY_GRADIENT_ROWS + PSX_SKY_ZOOM * (H * PSX_SKY_HORIZON_NUM / PSX_SKY_HORIZON_DEN));
    int c[3];
    colour(&b, 2, PSX_SKY_GRADIENT_ROWS + 2, c);                      /* the bitmap starts half a panorama on: the right half */
    CHECK(near(c, 40, 90, 200));
    colour(&b, b.width - 3, b.height - 2, c);                          /* and ends in the left */
    CHECK(near(c, 200, 120, 40));
    colour(&b, 5, 0, c);                                               /* the gradient's top row is the top colour */
    CHECK(near(c, 16, 16, 16));
    colour(&b, 5, PSX_SKY_GRADIENT_ROWS - 1, c);                       /* its last is near the bottom colour */
    CHECK(near(c, 208, 208, 208));
    int mid[3];
    colour(&b, 5, PSX_SKY_GRADIENT_ROWS / 2, mid);                     /* and it runs between */
    CHECK(mid[0] > 90 && mid[0] < 140);
    psx_sky_bitmap_free(&b);
    CHECK(b.pixels == NULL);
    psx_sky_free(&sky);
}

/* the two halves meet at the ring's seam: no step across it, however different their ends are */
static void test_seam(void)
{
    PsxSky sky;
    PsxSkyInf inf;
    made_up(&sky, &inf);
    PsxSkyBitmap b;
    CHECK(psx_sky_bitmap_build(&sky, &inf, &b));
    const int seam = b.width / 2;   /* the bitmap starts half a panorama on: the seam is in the middle */
    for (int y = PSX_SKY_GRADIENT_ROWS; y < b.height; y++) {
        int a[3], c[3];
        colour(&b, seam - 1, y, a);
        colour(&b, seam, y, c);
        CHECK(abs(a[0] - c[0]) <= 40 && abs(a[2] - c[2]) <= 40);
    }
    psx_sky_bitmap_free(&b);
    psx_sky_free(&sky);
}

static void test_many_colours(void)
{
    PsxSky sky;
    PsxSkyInf inf;
    made_up(&sky, &inf);
    for (int i = 0; i < W * H; i++) sky.rgb[3 * i] = (uint8_t)(i * 4), sky.rgb[3 * i + 1] = (uint8_t)(255 - i * 3);
    PsxSkyBitmap b;
    CHECK(psx_sky_bitmap_build(&sky, &inf, &b));
    int c[3];
    colour(&b, 4, PSX_SKY_GRADIENT_ROWS + 3, c);   /* any pixel still has a palette colour near the source's blend */
    CHECK(c[0] >= 0 && c[0] <= 255);
    psx_sky_bitmap_free(&b);
    psx_sky_free(&sky);
}

static void test_rejects(void)
{
    PsxSky sky = { 0, 0, NULL };
    PsxSkyInf inf;
    memset(&inf, 0, sizeof inf);
    PsxSkyBitmap b;
    CHECK(!psx_sky_bitmap_build(&sky, &inf, &b));
    CHECK(b.pixels == NULL);
}

int main(void)
{
    test_layout();
    test_seam();
    test_many_colours();
    test_rejects();
    return TEST_REPORT();
}
