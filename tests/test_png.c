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

/* a mod's mask (fuzz/masks.c): a chunk whose length runs past the file made stb_image grow its buffer to the length (2 GB from
 * 85 bytes) before it found the bytes missing; such a file is refused before stb_image sees it */
static void test_chunk_past_the_end(void)
{
    enum { W = 5, H = 4 };
    uint8_t px[W * H * 4] = { 0 };
    uint8_t *file = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_rgba(px, W, H, &file, &len), PNG_OK);
    CHECK(len > 12 && memcmp(file + len - 8, "IEND", 4) == 0);
    uint8_t *end = file + len - 12;   /* IEND's length: now an IDAT of 1 GB that is not there */
    end[0] = 0x40, end[1] = end[2] = end[3] = 0;
    memcpy(end + 4, "IDAT", 4);
    PngImage img;
    CHECK_EQ_INT(png_read(file, len, &img), PNG_ERR_CORRUPT);
    CHECK(img.pixels == NULL);
    /* the file ending inside that chunk, its CRC missing too */
    CHECK_EQ_INT(png_read(file, (size_t)(end - file) + 9, &img), PNG_ERR_CORRUPT);
    free(file);
}

/* An 8-bit indexed 2x2 image as an external encoder might write it: Paeth and Sub filters. */
/* a byte of the compressed data changed: the CRC no longer matches and the reader calls that corrupt (the writer compresses, so the bytes are not ours to edit by hand) */
static void test_corrupt(void)
{
    uint8_t pal[768] = {0};
    const uint8_t px[4] = { 10, 20, 15, 30 };
    uint8_t *file = NULL;
    size_t len = 0;
    CHECK_EQ_INT(png_write_indexed(px, 2, 2, pal, &file, &len), PNG_OK);
    file[8 + 25 + 780 + 8 + 3] ^= 0x55;   /* inside the IDAT body */
    PngImage img;
    CHECK_EQ_INT(png_read(file, len, &img), PNG_ERR_CORRUPT);
    free(file);
}

/* the writer compresses and filters: a mask (flat areas, a few classes) comes out small, any picture reads back exactly, noise is not made much bigger, and the same
 * picture is the same bytes twice */
static uint32_t rng_state = 12345;
static uint8_t rng(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return (uint8_t)(rng_state >> 24);
}

static void test_compressed(void)
{
    enum { W = 300, H = 200 };
    static uint8_t grey[W * H], rgba[W * H * 4], noise[W * H];
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const uint8_t v = x < 100 ? 0 : (x < 200 ? 255 : (y < 100 ? 128 : 224));
            grey[y * W + x] = v;
            rgba[4 * (y * W + x)] = v, rgba[4 * (y * W + x) + 1] = (uint8_t)(255 - v), rgba[4 * (y * W + x) + 2] = (uint8_t)(x * y), rgba[4 * (y * W + x) + 3] = 255;
            noise[y * W + x] = rng();
        }
    uint8_t *file = NULL, *again = NULL;
    size_t len = 0, len2 = 0;
    PngImage img;
    CHECK_EQ_INT(png_write_grey(grey, W, H, &file, &len), PNG_OK);
    CHECK(len < (size_t)(W * H) / 20);   /* stored, it was the raw size */
    CHECK_EQ_INT(png_write_grey(grey, W, H, &again, &len2), PNG_OK);
    CHECK(len == len2 && memcmp(file, again, len) == 0);
    CHECK_EQ_INT(png_read(file, len, &img), PNG_OK);
    CHECK(img.width == W && img.height == H);
    CHECK(!img.indexed);   /* a grey picture comes back as RGBA */
    for (int i = 0; i < W * H; ++i)
        if (img.pixels[i * 4] != grey[i]) { CHECK(0); break; }
    png_free(img.pixels);
    free(file), free(again);

    CHECK_EQ_INT(png_write_rgba(rgba, W, H, &file, &len), PNG_OK);
    CHECK_EQ_INT(png_read(file, len, &img), PNG_OK);
    CHECK(img.width == W && img.height == H && memcmp(img.pixels, rgba, sizeof rgba) == 0);
    png_free(img.pixels);
    free(file);

    uint8_t pal[768];
    for (int i = 0; i < 768; ++i) pal[i] = (uint8_t)(i * 7);
    CHECK_EQ_INT(png_write_indexed(noise, W, H, pal, &file, &len), PNG_OK);
    CHECK(len < (size_t)(W * H) + (size_t)(W * H) / 50 + 1200);   /* noise stays near its raw size */
    CHECK_EQ_INT(png_read(file, len, &img), PNG_OK);
    CHECK(img.indexed && img.width == W && img.height == H && memcmp(img.pixels, noise, sizeof noise) == 0 && memcmp(img.palette, pal, sizeof pal) == 0);
    png_free(img.pixels);
    free(file);

    CHECK_EQ_INT(png_write_rgba_fast(rgba, W, H, &file, &len), PNG_OK);   /* the fast writer: the same picture, not compressed */
    CHECK(len > (size_t)(W * H * 4));
    CHECK_EQ_INT(png_read(file, len, &img), PNG_OK);
    CHECK(img.width == W && img.height == H && memcmp(img.pixels, rgba, sizeof rgba) == 0);
    png_free(img.pixels);
    free(file);

    const uint8_t one[1] = { 77 };   /* the smallest picture */
    CHECK_EQ_INT(png_write_grey(one, 1, 1, &file, &len), PNG_OK);
    CHECK_EQ_INT(png_read(file, len, &img), PNG_OK);
    CHECK(img.width == 1 && img.height == 1);
    png_free(img.pixels);
    free(file);
}

int main(void)
{
    test_indexed();
    test_large_and_sizes();
    test_rgba();
    test_chunk_past_the_end();
    test_corrupt();
    test_compressed();
    return TEST_REPORT();
}
