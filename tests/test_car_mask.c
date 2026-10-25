/* The hand-made car masks (car_mask.h): a draft written as a PNG reads back as it was, a mask of another size is scaled to the
 * texture's by nearest neighbour, any non-black texel keeps and a black one paints, and what is not a PNG is refused. */
#include "formats/png.h"
#include "game/race/car_mask.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { W = 5, H = 3 };

uint8_t *platform_read_file(const char *path, size_t *size)
{
    (void)path;
    *size = 0;
    return NULL;
}
void platform_free_file(uint8_t *data) { free(data); }

static void test_round_trip(void)
{
    uint8_t keep[W * H], back[W * H];
    for (int i = 0; i < W * H; i++) keep[i] = (uint8_t)(i % 3 == 0);
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(car_mask_write(keep, W, H, &png, &len));
    CHECK(png != NULL && len > 8);
    CHECK(car_mask_parse(png, len, W, H, back));
    CHECK_MEM(back, keep, sizeof keep);
    png_free(png);
}

/* the mask's texels are 0 / 255 in the file and nothing else */
static void test_only_black_and_white(void)
{
    uint8_t keep[W * H];
    for (int i = 0; i < W * H; i++) keep[i] = (uint8_t)(i & 1);
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(car_mask_write(keep, W, H, &png, &len));
    PngImage img;
    CHECK_EQ_INT(png_read(png, len, &img), PNG_OK);
    CHECK_EQ_INT((int)img.width, W);
    CHECK_EQ_INT((int)img.height, H);
    CHECK(img.indexed);
    for (int i = 0; i < W * H; i++) {
        const uint8_t *c = img.palette + 3 * img.pixels[i];
        CHECK_EQ_INT(c[0], keep[i] ? 255 : 0);
        CHECK_EQ_INT(c[1], c[0]);
        CHECK_EQ_INT(c[2], c[0]);
    }
    png_free(img.pixels);
    png_free(png);
}

/* an RGBA export: any channel above 0 keeps, black paints, the alpha does not matter */
static void test_rgba(void)
{
    uint8_t rgba[2 * 1 * 4] = { 0, 0, 0, 255, 0, 40, 0, 255 };
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_rgba(rgba, 2, 1, &png, &len), PNG_OK);
    uint8_t out[2];
    CHECK(car_mask_parse(png, len, 2, 1, out));
    CHECK_EQ_INT(out[0], 0);
    CHECK_EQ_INT(out[1], 1);
    png_free(png);
}

/* the low detail bank's texture is smaller: nearest neighbour */
static void test_scaled(void)
{
    const uint8_t keep[2 * 2] = { 1, 0, 0, 1 };
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(car_mask_write(keep, 2, 2, &png, &len));
    uint8_t big[4 * 4];
    CHECK(car_mask_parse(png, len, 4, 4, big));
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) CHECK_EQ_INT(big[y * 4 + x], keep[(y / 2) * 2 + x / 2]);
    uint8_t small[1];
    CHECK(car_mask_parse(png, len, 1, 1, small));
    CHECK_EQ_INT(small[0], 1);   /* the texel at the top left */
    png_free(png);
}

static void test_refused(void)
{
    uint8_t out[4];
    CHECK(!car_mask_parse((const uint8_t *)"not a png", 9, 2, 2, out));
    CHECK(!car_mask_parse(NULL, 0, 2, 2, out));
    const uint8_t keep[4] = { 1, 1, 1, 1 };
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(car_mask_write(keep, 2, 2, &png, &len));
    CHECK(!car_mask_parse(png, len, 0, 2, out));
    CHECK(!car_mask_parse(png, len / 2, 2, 2, out));   /* cut short */
    png_free(png);
    CHECK(car_mask_load("TSupra", "rsid", 4, 4) == NULL);   /* no file */
}

int main(void)
{
    test_round_trip();
    test_only_black_and_white();
    test_rgba();
    test_scaled();
    test_refused();
    return TEST_REPORT();
}
