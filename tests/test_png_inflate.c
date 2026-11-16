/* PNG pixel data is inflated only up to the size its header implies (formats/png.h): a small file whose IDAT inflates past that (a
 * "zip bomb" in a mod's card, mask or skin) is refused at once as PNG_ERR_PAST_ROWS, without the memory; one that inflates to less is
 * refused as damaged,
 * and a header with more than PNG_MAX_PIXELS pixels is refused before anything is allocated. argv[1]: tests/png, small PNGs of other
 * encoders (bit depths 1 to 16, every colour type, Adam7 interlace) that must still read. */
#include "formats/crc32.h"
#include "formats/png.h"
#include "test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if !defined(_WIN32)
#include <sys/resource.h>
#endif

enum { BOMB_BYTES = 100 * 1024 * 1024, MATCH = 258 };

typedef struct { uint8_t *buf; size_t bits, cap; } Bits;

static void put_bit(Bits *b, int bit)
{
    if (bit) b->buf[b->bits >> 3] |= (uint8_t)(1u << (b->bits & 7));
    b->bits++;
}

static void put_code(Bits *b, unsigned code, int len)   /* a Huffman code: its most significant bit first */
{
    for (int i = len - 1; i >= 0; i--) put_bit(b, (code >> i) & 1);
}

/* a zlib stream of `n` zero bytes in one fixed-Huffman block: a literal 0, then matches of 258 at distance 1, then literals; malloc'd */
static uint8_t *zeros_zlib(size_t n, size_t *len)
{
    const size_t matches = n > 0 ? (n - 1) / MATCH : 0, literals = n > 0 ? 1 + (n - 1) % MATCH : 0;
    const size_t cap = 2 + (3 + 13 * matches + 8 * literals + 7) / 8 + 1 + 4;
    uint8_t *z = calloc(cap, 1);
    if (z == NULL) return NULL;
    z[0] = 0x78, z[1] = 0x01;
    Bits b = { z + 2, 0, cap - 6 };
    put_bit(&b, 1), put_bit(&b, 1), put_bit(&b, 0);   /* BFINAL, BTYPE 01 (fixed), its bits least significant first */
    if (n > 0) put_code(&b, 0x30, 8);                 /* literal 0 */
    for (size_t i = 0; i < matches; i++) put_code(&b, 0xc5, 8), put_code(&b, 0, 5);   /* length 258 (285), distance 1 (0) */
    for (size_t i = 1; i < literals; i++) put_code(&b, 0x30, 8);
    put_code(&b, 0, 7);                               /* end of block (256) */
    size_t at = 2 + (b.bits + 7) / 8;
    const uint32_t adler = (uint32_t)(n % 65521) << 16 | 1u;   /* zeros: a stays 1, b adds it once a byte */
    for (int i = 3; i >= 0; i--) z[at++] = (uint8_t)(adler >> (8 * i));
    *len = at;
    return z;
}

static uint8_t *chunk(uint8_t *at, const char *type, const uint8_t *data, size_t n)
{
    at[0] = (uint8_t)(n >> 24), at[1] = (uint8_t)(n >> 16), at[2] = (uint8_t)(n >> 8), at[3] = (uint8_t)n;
    memcpy(at + 4, type, 4);
    if (n > 0) memcpy(at + 8, data, n);
    const uint32_t crc = crc32_update(0, at + 4, 4 + n);
    for (int i = 0; i < 4; i++) at[8 + n + i] = (uint8_t)(crc >> (24 - 8 * i));
    return at + 12 + n;
}

/* a PNG of that header whose one IDAT inflates to `raw` zero bytes (filter 0 on every row); malloc'd */
static uint8_t *zero_png(uint32_t w, uint32_t h, int depth, int color, int interlace, size_t raw, size_t *len)
{
    size_t zlen = 0;
    uint8_t *z = zeros_zlib(raw, &zlen);
    uint8_t *file = z != NULL ? malloc(8 + 25 + 12 + 3 + 12 + zlen + 12) : NULL;
    if (file == NULL) { free(z); return NULL; }
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
    const uint8_t ihdr[13] = { (uint8_t)(w >> 24), (uint8_t)(w >> 16), (uint8_t)(w >> 8), (uint8_t)w, (uint8_t)(h >> 24), (uint8_t)(h >> 16),
                               (uint8_t)(h >> 8), (uint8_t)h, (uint8_t)depth, (uint8_t)color, 0, 0, (uint8_t)interlace };
    const uint8_t plte[3] = { 10, 20, 30 };
    memcpy(file, sig, 8);
    uint8_t *at = chunk(file + 8, "IHDR", ihdr, 13);
    if (color == 3) at = chunk(at, "PLTE", plte, 3);
    at = chunk(at, "IDAT", z, zlen);
    at = chunk(at, "IEND", NULL, 0);
    free(z);
    *len = (size_t)(at - file);
    return file;
}

/* the raw size, counted pixel by pixel through each Adam7 pass (or the one pass), not by png.c's formula */
static size_t raw_size(uint32_t w, uint32_t h, int depth, int color, int interlace)
{
    static const int channels[7] = { 1, 0, 3, 1, 2, 0, 4 };
    static const int pass[7][4] = { { 0, 0, 8, 8 }, { 4, 0, 8, 8 }, { 0, 4, 4, 8 }, { 2, 0, 4, 4 }, { 0, 2, 2, 4 }, { 1, 0, 2, 2 }, { 0, 1, 1, 2 } };
    const int passes = interlace ? 7 : 1;
    size_t total = 0;
    for (int p = 0; p < passes; p++) {
        const int x0 = interlace ? pass[p][0] : 0, y0 = interlace ? pass[p][1] : 0, dx = interlace ? pass[p][2] : 1, dy = interlace ? pass[p][3] : 1;
        size_t cols = 0, rows = 0;
        for (uint32_t x = (uint32_t)x0; x < w; x += (uint32_t)dx) cols++;
        for (uint32_t y = (uint32_t)y0; y < h; y += (uint32_t)dy) rows++;
        if (cols == 0 || rows == 0) continue;
        total += rows * (1 + (cols * (size_t)(depth * channels[color]) + 7) / 8);
    }
    return total;
}

static long peak_bytes(void)
{
#if defined(_WIN32)
    return 0;
#else
    struct rusage u;
    getrusage(RUSAGE_SELF, &u);
#if defined(__APPLE__)
    return (long)u.ru_maxrss;
#else
    return (long)u.ru_maxrss * 1024;
#endif
#endif
}

/* 16x16, indexed (our own reader) and RGBA (stb_image), each with an IDAT of 100 MB of zeros: refused at once, the memory never taken */
static void test_bombs(void)
{
    const int colors[2] = { 3, 6 };
    for (int i = 0; i < 2; i++) {
        size_t len = 0;
        uint8_t *file = zero_png(16, 16, 8, colors[i], 0, BOMB_BYTES, &len);
        CHECK(file != NULL && len < 1024 * 1024);
        if (file == NULL) continue;
        const long before = peak_bytes();
        const clock_t start = clock();
        PngImage img;
        CHECK_EQ_INT(png_read(file, len, &img), PNG_ERR_PAST_ROWS);
        CHECK(img.pixels == NULL);
        const double seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
        const long grew = peak_bytes() - before;
        printf("bomb, colour type %d: %zu bytes, refused in %.3f s, peak memory +%ld KB\n", colors[i], len, seconds, grew / 1024);
        CHECK(seconds < 2.0);
        CHECK(grew < 16L * 1024 * 1024);
        free(file);
    }
}

/* every kind of header: exactly its raw size reads, one byte less is damaged, one byte more is data past the rows */
static void test_exact_size(void)
{
    static const struct { uint32_t w, h; int depth, color, interlace; } KINDS[] = {
        { 16, 16, 8, 3, 0 }, { 13, 7, 1, 0, 0 }, { 11, 9, 4, 3, 0 }, { 9, 5, 16, 2, 0 }, { 13, 11, 8, 6, 1 }, { 3, 2, 8, 4, 1 },
        { 10, 1, 16, 0, 1 }, { 9, 9, 2, 3, 1 }, { 1, 1, 8, 6, 1 }, { 5, 3, 16, 6, 0 }, { 33, 17, 1, 3, 1 },
    };
    for (size_t k = 0; k < sizeof KINDS / sizeof KINDS[0]; k++) {
        const size_t raw = raw_size(KINDS[k].w, KINDS[k].h, KINDS[k].depth, KINDS[k].color, KINDS[k].interlace);
        for (int delta = -1; delta <= 1; delta++) {
            size_t len = 0;
            uint8_t *file = zero_png(KINDS[k].w, KINDS[k].h, KINDS[k].depth, KINDS[k].color, KINDS[k].interlace, raw + (size_t)delta, &len);
            PngImage img;
            const PngResult r = file != NULL ? png_read(file, len, &img) : PNG_ERR_MEMORY;
            if (delta == 0) {
                CHECK_EQ_INT(r, PNG_OK);
                if (r == PNG_OK) { CHECK_EQ_INT(img.width, KINDS[k].w); CHECK_EQ_INT(img.height, KINDS[k].h); png_free(img.pixels); }
            } else {
                CHECK_EQ_INT(r, delta > 0 ? PNG_ERR_PAST_ROWS : PNG_ERR_CORRUPT);
            }
            if (r != PNG_OK && r != PNG_ERR_CORRUPT && r != PNG_ERR_PAST_ROWS) printf("kind %zu delta %d: %s\n", k, delta, png_result_str(r));
            free(file);
        }
    }
}

/* more than PNG_MAX_PIXELS: refused by size, before the pixels (or the inflate) take memory */
static void test_too_many_pixels(void)
{
    const int colors[2] = { 3, 6 };
    for (int i = 0; i < 2; i++) {
        size_t len = 0;
        uint8_t *file = zero_png(2049, 2048, 8, colors[i], 0, 64, &len);
        const long before = peak_bytes();
        PngImage img;
        CHECK(file != NULL && png_read(file, len, &img) == PNG_ERR_SIZE);
        CHECK(peak_bytes() - before < 16L * 1024 * 1024);
        free(file);
        file = zero_png(16384, 2, 8, colors[i], 0, raw_size(16384, 2, 8, colors[i], 0), &len);   /* wide but few pixels: fine */
        CHECK(file != NULL && png_read(file, len, &img) == PNG_OK);
        if (file != NULL) png_free(img.pixels);
        free(file);
        file = zero_png(2048, 2048, 8, colors[i], 0, raw_size(2048, 2048, 8, colors[i], 0), &len);   /* at the limit: reads */
        CHECK(file != NULL && png_read(file, len, &img) == PNG_OK);
        if (file != NULL) png_free(img.pixels);
        free(file);
    }
    CHECK_EQ_INT(PNG_MAX_PIXELS, 2048 * 2048);
    CHECK(strstr(png_result_str(PNG_ERR_PAST_ROWS), "data past the image rows") != NULL);
}

static void test_fixtures(const char *dir)
{
    static const struct { const char *file; uint32_t w, h; } FIXTURES[] = {
        { "gray1.png", 13, 7 }, { "pal4.png", 11, 9 }, { "rgb16.png", 9, 5 }, { "rgba8_adam7.png", 13, 11 },
        { "ga8_adam7.png", 3, 2 }, { "gray16_adam7.png", 10, 1 }, { "pal2_adam7.png", 9, 9 },
    };
    for (size_t i = 0; i < sizeof FIXTURES / sizeof FIXTURES[0]; i++) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", dir, FIXTURES[i].file);
        FILE *f = fopen(path, "rb");
        uint8_t buf[4096];
        const size_t len = f != NULL ? fread(buf, 1, sizeof buf, f) : 0;
        if (f != NULL) fclose(f);
        CHECK(len > 0);
        PngImage img;
        const PngResult r = png_read(buf, len, &img);
        if (r != PNG_OK) printf("%s: %s\n", FIXTURES[i].file, png_result_str(r));
        CHECK_EQ_INT(r, PNG_OK);
        if (r != PNG_OK) continue;
        CHECK_EQ_INT(img.width, FIXTURES[i].w);
        CHECK_EQ_INT(img.height, FIXTURES[i].h);
        png_free(img.pixels);
    }
}

/* CgBI (Apple's PNG) after IHDR: stb_image still reads the IDAT as a deflate stream with no zlib header, so the size check must read it
 * so too. The stream here is that kind: exactly the rows reads, the 100 MB one is refused at once. */
static void test_apple_after_ihdr(void)
{
    const size_t raw = raw_size(16, 16, 8, 6, 0);
    const size_t sizes[2] = { raw, BOMB_BYTES };
    for (int i = 0; i < 2; i++) {
        size_t len = 0;
        uint8_t *file = zero_png(16, 16, 8, 6, 0, sizes[i], &len);
        CHECK(file != NULL);
        if (file == NULL) continue;
        /* the IDAT body without its zlib header and Adler, a CgBI chunk put between IHDR and it */
        const size_t ihdr_end = 8 + 25, idat_n = (size_t)file[ihdr_end] << 24 | (size_t)file[ihdr_end + 1] << 16 |
                                                 (size_t)file[ihdr_end + 2] << 8 | file[ihdr_end + 3];
        uint8_t *apple = malloc(len + 12);
        CHECK(apple != NULL);
        if (apple == NULL) { free(file); continue; }
        memcpy(apple, file, ihdr_end);
        uint8_t *at = chunk(apple + ihdr_end, "CgBI", NULL, 0);
        at = chunk(at, "IDAT", file + ihdr_end + 8 + 2, idat_n - 6);
        at = chunk(at, "IEND", NULL, 0);
        const long before = peak_bytes();
        PngImage img;
        const PngResult r = png_read(apple, (size_t)(at - apple), &img);
        CHECK_EQ_INT(r, i == 0 ? PNG_OK : PNG_ERR_PAST_ROWS);
        if (r == PNG_OK) png_free(img.pixels);
        CHECK(peak_bytes() - before < 16L * 1024 * 1024);
        free(apple);
        free(file);
    }
}

int main(int argc, char **argv)
{
    test_bombs();   /* first: the peak memory is a high-water mark */
    test_apple_after_ihdr();
    test_exact_size();
    test_too_many_pixels();
    if (argc > 1) test_fixtures(argv[1]);
    else CHECK(!"usage: test_png_inflate <tests/png>");
    return TEST_REPORT();
}
