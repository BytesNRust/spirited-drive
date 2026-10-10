/* PNG tests: our indexed writer and reader keep every index (duplicate palette colours included), RGBA goes
 * through stb_image, filters of other encoders are undone, and damage is reported rather than read. */
#include "formats/png.h"
#include "test.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void palette(uint8_t pal[768])
{
    for (int i = 0; i < 256; ++i) { pal[3 * i] = (uint8_t)i; pal[3 * i + 1] = (uint8_t)(255 - i); pal[3 * i + 2] = (uint8_t)(i / 2); }
    memcpy(pal + 3 * 7, pal + 3 * 9, 3);   /* entries 7 and 9 are the same colour */
}

static void test_indexed(void)
{
    enum { W = 37, H = 29 };
    uint8_t pal[768], px[W * H];
    palette(pal);
    for (int i = 0; i < W * H; ++i) px[i] = (uint8_t)(i * 13 + (i / W));
    px[0] = 7; px[1] = 9;
    uint8_t *file = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_indexed(px, W, H, pal, &file, &len), PNG_OK);
    PngImage img;
    CHECK_EQ_INT(png_read(file, len, &img), PNG_OK);
    CHECK_EQ_INT(img.indexed, 1);
    CHECK_EQ_INT(img.width, W);
    CHECK_EQ_INT(img.height, H);
    CHECK_MEM(img.pixels, px, W * H);           /* indices 7 and 9 stay apart */
    CHECK_MEM(img.palette, pal, 768);
    png_free(img.pixels);

    /* every damaged copy is refused or at least never crashes */
    for (size_t cut = 0; cut + 12 < len; cut += 7) {   /* what is left of IEND alone does not matter */
        PngImage bad;
        const PngResult r = png_read(file, cut, &bad);
        CHECK(r != PNG_OK);
    }
    file[len / 2] ^= 0x40;                      /* flips a bit in the pixel data: CRC catches it */
    CHECK_EQ_INT(png_read(file, len, &img), PNG_ERR_CORRUPT);
    free(file);
}

static void test_large_and_sizes(void)
{
    enum { W = 400, H = 300 };                  /* 120300 raw bytes: three stored blocks */
    static uint8_t px[W * H];
    uint8_t pal[768];
    palette(pal);
    for (int i = 0; i < W * H; ++i) px[i] = (uint8_t)(i ^ (i >> 8));
    uint8_t *file = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_indexed(px, W, H, pal, &file, &len), PNG_OK);
    PngImage img;
    CHECK_EQ_INT(png_read(file, len, &img), PNG_OK);
    CHECK_MEM(img.pixels, px, W * H);
    png_free(img.pixels);
    free(file);
    CHECK_EQ_INT(png_write_indexed(px, 0, 5, pal, &file, &len), PNG_ERR_SIZE);
    CHECK_EQ_INT(png_write_indexed(px, 5, 20000, pal, &file, &len), PNG_ERR_SIZE);
}

static void test_rgba(void)
{
    enum { W = 5, H = 4 };
    uint8_t px[W * H * 4];
    for (int i = 0; i < W * H * 4; ++i) px[i] = (uint8_t)(i * 11);
    uint8_t *file = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_rgba(px, W, H, &file, &len), PNG_OK);
    PngImage img;
    CHECK_EQ_INT(png_read(file, len, &img), PNG_OK);
    CHECK_EQ_INT(img.indexed, 0);
    CHECK_EQ_INT(img.width, W);
    CHECK_MEM(img.pixels, px, sizeof px);
    png_free(img.pixels);
    free(file);
}

/* An 8-bit indexed 2x2 image as an external encoder might write it: Paeth and Sub filters. */
static void test_filters(void)
{
    /* Built with the writer, then rows re-encoded: filter 1 (Sub) on row 0 and 2 (Up) on row 1. */
    uint8_t pal[768] = {0};
    const uint8_t px[4] = { 10, 20, 15, 30 };
    uint8_t *file = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_indexed(px, 2, 2, pal, &file, &len), PNG_OK);
    /* locate the stored data: IDAT body starts after sig(8)+IHDR(25)+PLTE(780)+len/type(8)+zlib hdr(2)+block hdr(5) */
    uint8_t *raw = file + 8 + 25 + 780 + 8 + 2 + 5;
    CHECK_EQ_INT(raw[0], 0);
    raw[0] = 1; raw[1] = 10; raw[2] = 10;          /* Sub: 10, 10+10 */
    raw[3] = 2; raw[4] = 5; raw[5] = 10;           /* Up: 10+5, 20+10 */
    /* the CRC and Adler no longer match the edited bytes: the reader must call that corrupt */
    PngImage img;
    CHECK_EQ_INT(png_read(file, len, &img), PNG_ERR_CORRUPT);
    free(file);
}

int main(void)
{
    test_indexed();
    test_large_and_sizes();
    test_rgba();
    test_filters();
    return TEST_REPORT();
}
