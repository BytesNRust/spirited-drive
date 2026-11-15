/* dep59 tools B2, Task 4: a PNG fitted to a car bank's palette, keeping the race's own indexes (formats/car_texture.h). On a
 * palette of the test's own: an RGBA picture's exact colours keep their indexes and a transparent pixel is 0xFF; a colour the
 * palette lacks goes to the nearest, never past 0xF9; an indexed PNG drawn with the bank's palette keeps 0xFA..0xFF; a 6-bit
 * palette and its 8-bit twin give the same indexes; the entry names. Needs no CD. */
#include "formats/car_texture.h"
#include "formats/png.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* a 6-bit palette: index i is (i, 63 - i/4, i/2) and the specials 0xFA..0xFF are bright; its 8-bit twin is each byte << 2 */
static void palettes(uint8_t six[SHPI_PALETTE_BYTES], uint8_t eight[SHPI_PALETTE_BYTES])
{
    for (int i = 0; i < 256; i++) {
        six[3 * i] = (uint8_t)(i % 64), six[3 * i + 1] = (uint8_t)(63 - i / 4), six[3 * i + 2] = (uint8_t)(i / 2 % 64);
        if (i >= 0xfa) six[3 * i] = six[3 * i + 1] = six[3 * i + 2] = 63;
    }
    for (int i = 0; i < SHPI_PALETTE_BYTES; i++) eight[i] = (uint8_t)(six[i] << 2);
}

static uint8_t *fitted(const uint8_t *png, size_t len, const uint8_t *pal, uint8_t type, CarTextureStats *s)
{
    uint8_t *px = NULL;
    uint16_t w = 0, h = 0;
    ExchangeError err = { { 0 } };
    if (!car_texture_from_png(png, len, pal, type, &px, &w, &h, s, &err)) fprintf(stderr, "car_texture: %s\n", err.message);
    return px;
}

/* 4 x 2 RGBA: index 10, 20, 0xF9's and 100's colours exactly, a transparent pixel, white (no such colour: 0xFA..0xFF are white),
 * a colour near index 30, black */
static void test_rgba(const uint8_t six[SHPI_PALETTE_BYTES], const uint8_t eight[SHPI_PALETTE_BYTES])
{
    uint8_t rgba[8 * 4];
    static const int FROM[8] = { 10, 20, 0xf9, 100, -1, -2, 30, -3 };
    for (int k = 0; k < 8; k++) {
        uint8_t *p = rgba + 4 * k;
        const int i = FROM[k];
        if (i >= 0) p[0] = eight[3 * i], p[1] = eight[3 * i + 1], p[2] = eight[3 * i + 2], p[3] = 255;
        if (i == -1) p[0] = 9, p[1] = 9, p[2] = 9, p[3] = 0;
        if (i == -2) p[0] = p[1] = p[2] = p[3] = 255;
        if (i == -3) p[0] = p[1] = p[2] = 0, p[3] = 255;
        if (k == 6) p[0] = (uint8_t)(p[0] + 1);   /* near 30, not on it */
    }
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(png_write_rgba(rgba, 4, 2, &png, &len) == PNG_OK);
    CarTextureStats s6, s8;
    uint8_t *a = fitted(png, len, six, 0x22, &s6), *b = fitted(png, len, eight, 0x24, &s8);
    CHECK(a != NULL && b != NULL);
    if (a != NULL && b != NULL) {
        CHECK_EQ_INT(a[0], 10);
        CHECK_EQ_INT(a[1], 20);
        CHECK_EQ_INT(a[2], 0xf9);
        CHECK_EQ_INT(a[3], 100);
        CHECK_EQ_INT(a[4], 0xff);
        CHECK(a[5] <= 0xf9);   /* white: the palette's whites are the specials, never chosen */
        CHECK_EQ_INT(a[6], 30);
        CHECK(a[7] <= 0xf9);
        CHECK(memcmp(a, b, 8) == 0);   /* a 6-bit palette and its 8-bit twin */
        CHECK_EQ_INT((int)s6.pixels, 8);
        CHECK_EQ_INT((int)s6.transparent, 1);
        CHECK_EQ_INT((int)s6.kept_special, 0);
        CHECK_EQ_INT((int)s6.moved, 3);   /* white, near 30, black */
    }
    free(a), free(b), free(png);
}

/* an indexed PNG with the bank's palette keeps 0xFA..0xFF; with another palette its colours are fitted */
static void test_indexed(const uint8_t six[SHPI_PALETTE_BYTES], const uint8_t eight[SHPI_PALETTE_BYTES])
{
    const uint8_t idx[8] = { 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 7, 200 };
    uint8_t *png = NULL;
    size_t len = 0;
    CHECK(png_write_indexed(idx, 4, 2, eight, &png, &len) == PNG_OK);
    CarTextureStats s;
    uint8_t *a = fitted(png, len, six, 0x22, &s);
    CHECK(a != NULL && memcmp(a, idx, 8) == 0);
    CHECK_EQ_INT((int)s.kept_special, 6);
    CHECK_EQ_INT((int)s.colours, 8);
    free(a), free(png);
    uint8_t other[SHPI_PALETTE_BYTES];
    memcpy(other, eight, sizeof other);
    other[3 * 7] ^= 4;   /* one colour differs: not the bank's palette */
    CHECK(png_write_indexed(idx, 4, 2, other, &png, &len) == PNG_OK);
    a = fitted(png, len, six, 0x22, &s);
    CHECK(a != NULL);
    for (int k = 0; a != NULL && k < 8; k++) CHECK(a[k] <= 0xf9);
    if (a != NULL) CHECK_EQ_INT(a[7], 200);
    CHECK_EQ_INT((int)s.kept_special, 0);
    free(a), free(png);
}

static void test_refusals(const uint8_t six[SHPI_PALETTE_BYTES])
{
    uint8_t *px = NULL;
    uint16_t w, h;
    CarTextureStats s;
    ExchangeError err = { { 0 } };
    CHECK(!car_texture_from_png((const uint8_t *)"not a png", 9, six, 0x22, &px, &w, &h, &s, &err) && px == NULL);
    CHECK(strstr(err.message, "PNG") != NULL);
    uint8_t rgba[4] = { 1, 2, 3, 255 }, *png = NULL;
    size_t len = 0;
    CHECK(png_write_rgba(rgba, 1, 1, &png, &len) == PNG_OK);
    CHECK(!car_texture_from_png(png, len, six, 0x7b, &px, &w, &h, &s, &err) && px == NULL);
    CHECK(car_texture_from_png(png, len, six, 0x22, &px, &w, &h, &s, &err) && w == 1 && h == 1);
    free(px), free(png);
}

static void test_names(void)
{
    char a[4], b[4], c[4], d[4];
    car_texture_entry_name("body", NULL, 0, a);
    CHECK(memcmp(a, "body", 4) == 0);
    const char taken[2][4] = { { 'b', 'o', 'd', 'y' }, { 'r', 'o', 'o', 'f' } };
    car_texture_entry_name("bodywork", taken, 2, b);
    CHECK(memcmp(b, "body", 4) != 0 && memcmp(b, "roof", 4) != 0);
    car_texture_entry_name("bodywork", taken, 2, c);
    CHECK(memcmp(b, c, 4) == 0);   /* stable */
    car_texture_entry_name("!PAL", NULL, 0, d);
    CHECK(d[0] != '!');
    car_texture_entry_name("a b", NULL, 0, d);
    CHECK(memcmp(d, "a_b_", 4) == 0);
    const char full[3][4] = { { 'b', 'o', 'd', 'y' }, { 'b', 'o', 'd', '1' }, { 'b', 'o', 'd', '2' } };
    car_texture_entry_name("body", full, 3, d);
    CHECK(memcmp(d, "body", 4) != 0 && memcmp(d, "bod1", 4) != 0 && memcmp(d, "bod2", 4) != 0);
}

int main(void)
{
    uint8_t six[SHPI_PALETTE_BYTES], eight[SHPI_PALETTE_BYTES];
    palettes(six, eight);
    test_rgba(six, eight);
    test_indexed(six, eight);
    test_refusals(six);
    test_names();
    return TEST_REPORT();
}
