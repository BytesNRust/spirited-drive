/* The PS1 look's sky files (psx_sky.h) on made-up bytes in the real files' shape: the INF's text and the PSH's SHPP
 * directory of 8bpp horizon tiles, each through its own 256-entry CLUT. No CD data. */
#include "game/race/psx_sky.h"
#include "formats/psx_bigf.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static void put24(uint8_t *p, uint32_t v) { put16(p, v), p[2] = (uint8_t)(v >> 16); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v), put16(p + 2, v >> 16); }

enum {
    TILE_W = 4, TILE_H = 2,
    IMAGE_LEN = 16 + TILE_W * TILE_H, CLUT_LEN = 16 + 2 * 256, TEX_LEN = IMAGE_LEN + CLUT_LEN,
    HEAD = 16, STRIDE = 8, ENTRIES = 10, DIR = HEAD + STRIDE * ENTRIES   /* hrz0..hrz7, rhrz, grv2 */
};

/* A texture of TILE_W x TILE_H pixels, pixel i showing CLUT index i + 1, whose colour is `base` + i (BGR555). */
static void texture(uint8_t *p, uint8_t type, uint16_t base)
{
    memset(p, 0, TEX_LEN);
    p[0] = type, put24(p + 1, IMAGE_LEN), put16(p + 4, TILE_W), put16(p + 6, TILE_H);
    for (int i = 0; i < TILE_W * TILE_H; i++) p[16 + i] = (uint8_t)(i + 1);
    uint8_t *c = p + IMAGE_LEN;
    c[0] = 0x23, put24(c + 1, CLUT_LEN), put16(c + 4, 256), put16(c + 6, 1);
    for (int i = 0; i < TILE_W * TILE_H; i++) put16(c + 16 + 2 * (i + 1), (uint32_t)(base + i) | 0x8000);   /* STP set */
}

/* The names in order; tile k's colours start at 0x0021 * k (red = k, green = k, blue = 0 + the pixel). */
static size_t directory(uint8_t *f, int hrz_count, int type)
{
    memset(f, 0, DIR + ENTRIES * TEX_LEN);
    memcpy(f, "SHPP", 4), put32(f + 8, ENTRIES), memcpy(f + 12, "GIMX", 4);
    static const char *names[ENTRIES] = { "hrz0", "hrz1", "hrz2", "hrz3", "hrz4", "hrz5", "hrz6", "hrz7", "rhrz", "grv2" };
    for (int i = 0; i < ENTRIES; i++) {
        const char *name = names[i];
        if (i >= hrz_count && i < 8) name = "xxxx";   /* a missing tile */
        memcpy(f + HEAD + STRIDE * i, name, 4);
        put32(f + HEAD + STRIDE * i + 4, DIR + TEX_LEN * i);
        texture(f + DIR + TEX_LEN * i, (uint8_t)type, (uint16_t)(0x0021 * i));
    }
    return DIR + ENTRIES * TEX_LEN;
}

static void test_image8(void)
{
    static uint8_t f[DIR + ENTRIES * TEX_LEN];
    const size_t size = directory(f, 8, 0x41);
    PsxImage8 img;
    CHECK(psx_shpp_image8(f, size, "hrz2", &img));
    CHECK_EQ_INT(img.width, TILE_W);
    CHECK_EQ_INT(img.height, TILE_H);
    CHECK_EQ_INT(img.pixels[3], 4);
    uint8_t rgb[3];
    psx_clut_rgb(img.clut, img.pixels[3], rgb);   /* tile 2, pixel 3: 0x0042 + 3 = 0x0045: r 5, g 2, b 0 */
    CHECK_EQ_INT(rgb[0], 5 << 3);
    CHECK_EQ_INT(rgb[1], 2 << 3);
    CHECK_EQ_INT(rgb[2], 0);
    CHECK(!psx_shpp_image8(f, size, "nope", &img));
    CHECK(!psx_shpp_image8(f, 40, "hrz2", &img));       /* the directory is cut short */
    CHECK(!psx_shpp_image8(f, DIR + 4 * TEX_LEN + 20, "hrz4", &img));   /* so is the texture */
    CHECK_EQ_INT(psx_shpp_count(f, size), ENTRIES);
    uint8_t four[sizeof f];
    const size_t four_size = directory(four, 8, 0x40);
    CHECK(!psx_shpp_image8(four, four_size, "hrz0", &img));   /* 4bpp is not 8bpp */
}

static void test_panorama(void)
{
    static uint8_t f[DIR + ENTRIES * TEX_LEN];
    size_t size = directory(f, 8, 0x41);
    PsxSky sky;
    CHECK(psx_sky_panorama(f, size, &sky));
    CHECK_EQ_INT(sky.width, 8 * TILE_W);
    CHECK_EQ_INT(sky.height, TILE_H);
    const uint8_t *px = sky.rgb + 3 * (1 * sky.width + 3 * TILE_W + 2);   /* row 1, tile 3's column 2: pixel 6 */
    CHECK_EQ_INT(px[0], ((0x0021 * 3 + 6) & 31) << 3);
    CHECK_EQ_INT(px[1], (((0x0021 * 3 + 6) >> 5) & 31) << 3);
    CHECK_EQ_INT(px[2], 0);
    CHECK(psx_sky_checksum(sky.rgb, (size_t)sky.width * sky.height * 3) != 0);
    psx_sky_free(&sky);
    CHECK(sky.rgb == NULL);
    size = directory(f, 7, 0x41);   /* hrz7 missing */
    CHECK(!psx_sky_panorama(f, size, &sky));
    CHECK(sky.rgb == NULL);
}

static void test_inf(void)
{
    static const char TEXT[] =
        "# vert offset near\r\n0.5\r\n# vert offset far\r\n0.25\r\n# zoffset near\r\n1\r\n# zoffset far\r\n2\r\n"
        "# height near\r\n3\r\n# height far\r\n4\r\n# radius near\r\n5\r\n# radius far\r\n6\r\n"
        "# colour top\r\n48 42 26\r\n# colour bottom\r\n84 69 40\r\n# dithering flag\r\n1\r\n# ratio of the blend\r\n32\r\n"
        "# ring y offset\r\n-12\r\n# ring height\r\n100\r\n";
    PsxSkyInf inf;
    CHECK(psx_sky_inf_parse((const uint8_t *)TEXT, sizeof TEXT - 1, &inf));
    CHECK_EQ_INT(inf.top[0], 48);
    CHECK_EQ_INT(inf.top[2], 26);
    CHECK_EQ_INT(inf.bottom[0], 84);
    CHECK_EQ_INT(inf.bottom[2], 40);
    CHECK_EQ_INT(inf.dither, 1);
    CHECK_EQ_INT(inf.blend, 32);
    CHECK_EQ_INT(inf.ring_y, -12);
    CHECK_EQ_INT(inf.ring_height, 100);
    CHECK_EQ_INT(inf.fields, 14);
    CHECK_EQ_INT((int)(inf.geometry[0] * 100), 50);
    CHECK_EQ_INT((int)inf.geometry[7], 6);
    CHECK(!psx_sky_inf_parse((const uint8_t *)TEXT, 60, &inf));      /* the colours are not there */
    CHECK(!psx_sky_inf_parse((const uint8_t *)"# nothing\n", 10, &inf));
    CHECK(!psx_sky_inf_parse(NULL, 10, &inf));
    static const char SHORT[] = "0\n0\n0\n0\n0\n0\n0\n0\n1 2 3\n4 5 6\n";   /* gradient only: the rest stays 0 */
    CHECK(psx_sky_inf_parse((const uint8_t *)SHORT, sizeof SHORT - 1, &inf));
    CHECK_EQ_INT(inf.bottom[2], 6);
    CHECK_EQ_INT(inf.fields, 10);
    CHECK_EQ_INT(inf.ring_height, 0);
}

int main(void)
{
    test_image8();
    test_panorama();
    test_inf();
    return TEST_REPORT();
}
