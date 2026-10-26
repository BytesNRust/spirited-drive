/* The hand-made car masks (car_mask.h): a draft written as a PNG reads back as it was (paint, shine, matte), a mask of another size is scaled
 * to the texture's by nearest neighbour, colours are classed, and what is not a PNG is refused. */
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

/* the three values come back as they went in; any other non-zero value is matte */
static void test_round_trip(void)
{
    uint8_t mask[W * H], back[W * H];
    static const uint8_t CLASSES[3] = { CAR_MASK_PAINT, CAR_MASK_SHINE, CAR_MASK_MATTE };
    for (int i = 0; i < W * H; i++) mask[i] = CLASSES[i % 3];
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(car_mask_write(mask, W, H, &png, &len));
    CHECK(png != NULL && len > 8);
    CHECK(car_mask_parse(png, len, W, H, back));
    CHECK_MEM(back, mask, sizeof mask);
    png_free(png);
    const uint8_t odd[2] = { 1, 77 };   /* a draft's "keep" of 1 is matte, not a lost texel */
    CHECK(car_mask_write(odd, 2, 1, &png, &len));
    CHECK(car_mask_parse(png, len, 2, 1, back));
    CHECK_EQ_INT(back[0], CAR_MASK_MATTE);
    CHECK_EQ_INT(back[1], CAR_MASK_MATTE);
    png_free(png);
}

/* the file's texels are 0, 128 and 255 and nothing else, grey */
static void test_file_values(void)
{
    uint8_t mask[W * H];
    for (int i = 0; i < W * H; i++) mask[i] = (uint8_t)(i % 3 == 0 ? CAR_MASK_PAINT : i % 3 == 1 ? CAR_MASK_SHINE : CAR_MASK_MATTE);
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(car_mask_write(mask, W, H, &png, &len));
    PngImage img;
    CHECK_EQ_INT(png_read(png, len, &img), PNG_OK);
    CHECK_EQ_INT((int)img.width, W);
    CHECK_EQ_INT((int)img.height, H);
    CHECK(img.indexed);
    for (int i = 0; i < W * H; i++) {
        const uint8_t *c = img.palette + 3 * img.pixels[i];
        CHECK_EQ_INT(c[0], mask[i]);
        CHECK_EQ_INT(c[1], c[0]);
        CHECK_EQ_INT(c[2], c[0]);
    }
    png_free(img.pixels);
    png_free(png);
}

/* an export of another tool: black paints, a blue shines, a bright one is matte, a dim one shines; the alpha does not matter */
static void test_colours(void)
{
    uint8_t rgba[5 * 4] = { 0, 0, 0, 255, 0, 0, 255, 255, 255, 255, 255, 0, 200, 40, 40, 255, 128, 128, 128, 255 };
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_rgba(rgba, 5, 1, &png, &len), PNG_OK);
    uint8_t out[5];
    CHECK(car_mask_parse(png, len, 5, 1, out));
    CHECK_EQ_INT(out[0], CAR_MASK_PAINT);
    CHECK_EQ_INT(out[1], CAR_MASK_SHINE);
    CHECK_EQ_INT(out[2], CAR_MASK_MATTE);
    CHECK_EQ_INT(out[3], CAR_MASK_MATTE);
    CHECK_EQ_INT(out[4], CAR_MASK_SHINE);
    png_free(png);
}

/* the low detail bank's texture is smaller: nearest neighbour */
static void test_scaled(void)
{
    const uint8_t mask[2 * 2] = { CAR_MASK_MATTE, CAR_MASK_PAINT, CAR_MASK_SHINE, CAR_MASK_MATTE };
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(car_mask_write(mask, 2, 2, &png, &len));
    uint8_t big[4 * 4];
    CHECK(car_mask_parse(png, len, 4, 4, big));
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) CHECK_EQ_INT(big[y * 4 + x], mask[(y / 2) * 2 + x / 2]);
    uint8_t small[1];
    CHECK(car_mask_parse(png, len, 1, 1, small));
    CHECK_EQ_INT(small[0], CAR_MASK_MATTE);   /* the texel at the top left */
    png_free(png);
}

static void test_refused(void)
{
    uint8_t out[4];
    CHECK(!car_mask_parse((const uint8_t *)"not a png", 9, 2, 2, out));
    CHECK(!car_mask_parse(NULL, 0, 2, 2, out));
    const uint8_t mask[4] = { 255, 255, 255, 255 };
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(car_mask_write(mask, 2, 2, &png, &len));
    CHECK(!car_mask_parse(png, len, 0, 2, out));
    CHECK(!car_mask_parse(png, len / 2, 2, 2, out));   /* cut short */
    png_free(png);
    CHECK(car_mask_load("TSupra", "rsid", 4, 4) == NULL);   /* no file */
}

int main(void)
{
    test_round_trip();
    test_file_values();
    test_colours();
    test_scaled();
    test_refused();
    return TEST_REPORT();
}
