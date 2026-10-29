/* The hand-made car masks (car_mask.h): a draft written as a PNG reads back as it was (paint, shine, matte), a mask of another size is scaled
 * to the texture's by nearest neighbour, colours are classed, and what is not a PNG is refused. */
#include "formats/png.h"
#include "game/race/car_mask.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum { W = 5, H = 3 };

/* the lookup test reads real files (the directory of TNFS_CAR_MASKS) */
uint8_t *platform_read_file(const char *path, size_t *size)
{
    *size = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    long n = 0;
    uint8_t *d = NULL;
    if (fseek(f, 0, SEEK_END) == 0) n = ftell(f);
    if (n > 0 && fseek(f, 0, SEEK_SET) == 0) d = malloc((size_t)n);
    if (d != NULL && fread(d, 1, (size_t)n, f) == (size_t)n) *size = (size_t)n;
    else free(d), d = NULL;
    fclose(f);
    return d;
}
void platform_free_file(uint8_t *data) { free(data); }

/* the four values come back as they went in; any other non-zero value is matte */
static void test_round_trip(void)
{
    uint8_t mask[W * H], back[W * H];
    static const uint8_t CLASSES[4] = { CAR_MASK_PAINT, CAR_MASK_SHINE, CAR_MASK_MATTE, CAR_MASK_BODY };
    for (int i = 0; i < W * H; i++) mask[i] = CLASSES[i % 4];
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

/* the body class (192: keeps the texture's colour, shines like the body): a grey of 192 and a green-dominant colour; the other classes' greys and
 * the colours that were matte stay as they were (a bright red is still matte) */
static void test_the_body_class(void)
{
    uint8_t rgba[6 * 4] = { 192, 192, 192, 255, 40, 200, 40, 255, 0, 160, 0, 255, 255, 255, 255, 255, 200, 40, 40, 255, 128, 128, 128, 255 };
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_rgba(rgba, 6, 1, &png, &len), PNG_OK);
    uint8_t out[6];
    CHECK(car_mask_parse(png, len, 6, 1, out));
    CHECK_EQ_INT(out[0], CAR_MASK_BODY);
    CHECK_EQ_INT(out[1], CAR_MASK_BODY);
    CHECK_EQ_INT(out[2], CAR_MASK_BODY);
    CHECK_EQ_INT(out[3], CAR_MASK_MATTE);
    CHECK_EQ_INT(out[4], CAR_MASK_MATTE);
    CHECK_EQ_INT(out[5], CAR_MASK_SHINE);
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
    CHECK(car_mask_load("TNoSuch", "rsid", 0, 4, 4) == NULL);   /* no file */
}

#define ROOT "car_mask_test_masks"   /* the directory the test makes in the working directory */

static void write_mask(const char *car, const char *file, const uint8_t *mask, int width, int height)
{
    char path[256];
    snprintf(path, sizeof path, "%s/%s/%s", ROOT, car, file);
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(car_mask_write(mask, width, height, &png, &len));
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(fwrite(png, 1, len, f) == len);
        fclose(f);
    }
    png_free(png);
}

/* the high detail level reads <tex>_mask.png, the low one <tex>_lo_mask.png, and never the other's file: the low texture is of
 * another size, and its own mask is the one that marks it */
static void test_lookup_by_level(void)
{
    uint8_t high[W * H], low[2 * 2];
    memset(high, CAR_MASK_PAINT, sizeof high);
    memset(low, CAR_MASK_MATTE, sizeof low);
    mkdir(ROOT, 0755);
    mkdir(ROOT "/TLook", 0755);
    mkdir(ROOT "/THigh", 0755);
    write_mask("TLook", "rsid_mask.png", high, W, H);
    write_mask("TLook", "rsid_lo_mask.png", low, 2, 2);
    write_mask("THigh", "rsid_mask.png", high, W, H);
    setenv("TNFS_CAR_MASKS", ROOT, 1);
    uint8_t *m0 = car_mask_load("TLook", "rsid", 0, W, H);
    CHECK(m0 != NULL);
    if (m0 != NULL) CHECK_EQ_INT(m0[0], CAR_MASK_PAINT);
    free(m0);
    uint8_t *m1 = car_mask_load("TLook", "rsid", 1, 2, 2);
    CHECK(m1 != NULL);
    if (m1 != NULL) CHECK_EQ_INT(m1[0], CAR_MASK_MATTE);
    free(m1);
    CHECK(car_mask_load("THigh", "rsid", 1, 2, 2) == NULL);   /* only the high detail's file: the low detail has none */
}

int main(void)
{
    test_round_trip();
    test_file_values();
    test_colours();
    test_the_body_class();
    test_scaled();
    test_refused();
    test_lookup_by_level();
    return TEST_REPORT();
}
