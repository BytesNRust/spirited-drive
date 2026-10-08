/* FSH (SHPI bitmap container) tests. Lookup follows 004732dd (exact 4-byte
 * names), palette expansion follows 00463e08 (type 0x24 as is, else << 2). */
#include "formats/fsh.h"
#include "test.h"

#include <stdlib.h>

enum { ENTRY_HEADER = 16, PALETTE_BYTES = 768 };

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    put_u16(p, (uint16_t)v);
    put_u16(p + 2, (uint16_t)(v >> 16));
}

static void put_entry_header(uint8_t *p, uint8_t type, uint32_t block, uint16_t w, uint16_t h,
                             int16_t m0, int16_t m1, int16_t m2, int16_t m3)
{
    p[0] = type;
    p[1] = (uint8_t)block;
    p[2] = (uint8_t)(block >> 8);
    p[3] = (uint8_t)(block >> 16);
    put_u16(p + 4, w);
    put_u16(p + 6, h);
    put_u16(p + 8, (uint16_t)m0);
    put_u16(p + 10, (uint16_t)m1);
    put_u16(p + 12, (uint16_t)m2);
    put_u16(p + 14, (uint16_t)m3);
}

/* SHPI with "!pal" (type 0x22, 6-bit) and "bgnd" (4x2 bitmap at 10,20). */
static uint8_t *build_sample(size_t *size)
{
    const size_t pal_at = 16 + 2 * 8;
    const size_t bmp_at = pal_at + ENTRY_HEADER + PALETTE_BYTES;
    *size = bmp_at + ENTRY_HEADER + 8;
    uint8_t *f = calloc(1, *size);
    if (f == NULL) return NULL;
    memcpy(f, "SHPI", 4);
    put_u32(f + 4, (uint32_t)*size);
    put_u32(f + 8, 2);
    memcpy(f + 12, "LN32", 4);
    memcpy(f + 16, "!pal", 4);
    put_u32(f + 20, (uint32_t)pal_at);
    memcpy(f + 24, "bgnd", 4);
    put_u32(f + 28, (uint32_t)bmp_at);

    put_entry_header(f + pal_at, 0x22, 0, 256, 3, 0, 0, 0, 0);
    for (int i = 0; i < PALETTE_BYTES; i++) f[pal_at + ENTRY_HEADER + i] = (uint8_t)(i % 64);

    put_entry_header(f + bmp_at, 0x7B, ENTRY_HEADER + 8, 4, 2, -2, -3, 10, 20);
    for (int i = 0; i < 8; i++) f[bmp_at + ENTRY_HEADER + i] = (uint8_t)(i + 1);
    return f;
}

static void test_open_and_find(void)
{
    size_t size;
    uint8_t *data = build_sample(&size);
    CHECK(data != NULL);
    if (data == NULL) return;

    Fsh fsh;
    CHECK_EQ_INT(fsh_open(&fsh, data, size), FSH_OK);
    CHECK_EQ_INT(fsh.count, 2);

    uint32_t index = 99;
    CHECK_EQ_INT(fsh_find(&fsh, "bgnd", &index), FSH_OK);
    CHECK_EQ_INT(index, 1);
    CHECK_EQ_INT(fsh_find(&fsh, "!PAL", &index), FSH_ERR_NOT_FOUND);  /* case-sensitive like the game */
    free(data);
}

static void test_bitmap_entry(void)
{
    size_t size;
    uint8_t *data = build_sample(&size);
    if (data == NULL) return;
    Fsh fsh;
    FshEntry e = { { 0 }, 0, 0, 0, 0, { 0 }, NULL, 0 };
    fsh_open(&fsh, data, size);
    CHECK_EQ_INT(fsh_entry(&fsh, 1, &e), FSH_OK);
    CHECK_MEM(e.name, "bgnd", 4);
    CHECK_EQ_INT(e.type, FSH_TYPE_BITMAP);
    CHECK_EQ_INT(e.width, 4);
    CHECK_EQ_INT(e.height, 2);
    CHECK_EQ_INT(e.misc[0], -2);
    CHECK_EQ_INT(e.misc[2], 10);
    CHECK_EQ_INT(e.misc[3], 20);
    CHECK(e.payload != NULL && e.payload_size >= 8);
    if (e.payload != NULL && e.payload_size >= 8) {
        CHECK_EQ_INT(e.payload[0], 1);
        CHECK_EQ_INT(e.payload[7], 8);
    }
    CHECK_EQ_INT(fsh_entry(&fsh, 2, &e), FSH_ERR_BOUNDS);
    free(data);
}

static void test_palette(void)
{
    size_t size;
    uint8_t *data = build_sample(&size);
    if (data == NULL) return;
    Fsh fsh;
    FshEntry e;
    uint8_t rgb[PALETTE_BYTES];
    fsh_open(&fsh, data, size);

    CHECK_EQ_INT(fsh_entry(&fsh, 0, &e), FSH_OK);
    CHECK_EQ_INT(fsh_palette_rgb(&e, rgb), FSH_OK);
    CHECK_EQ_INT(rgb[1], 4);      /* 1 << 2 */
    CHECK_EQ_INT(rgb[63], 252);   /* VGA white stays 252, as in the game */

    data[16 + 16] = 0x24;         /* 8-bit palette: copied unchanged */
    CHECK_EQ_INT(fsh_entry(&fsh, 0, &e), FSH_OK);
    CHECK_EQ_INT(fsh_palette_rgb(&e, rgb), FSH_OK);
    CHECK_EQ_INT(rgb[63], 63);

    CHECK_EQ_INT(fsh_entry(&fsh, 1, &e), FSH_OK);
    CHECK_EQ_INT(fsh_palette_rgb(&e, rgb), FSH_ERR_TYPE);
    free(data);
}

static void test_corrupt(void)
{
    size_t size;
    uint8_t *data = build_sample(&size);
    if (data == NULL) return;
    Fsh fsh;
    FshEntry e;

    CHECK_EQ_INT(fsh_open(&fsh, data, 15), FSH_ERR_HEADER);
    CHECK_EQ_INT(fsh_open(&fsh, (const uint8_t *)"SHPX0000000000000000", 20), FSH_ERR_HEADER);

    put_u32(data + 8, 1000);  /* directory larger than the file */
    CHECK_EQ_INT(fsh_open(&fsh, data, size), FSH_ERR_BOUNDS);
    put_u32(data + 8, 2);

    fsh_open(&fsh, data, size - 1);  /* bitmap pixels cut short */
    CHECK_EQ_INT(fsh_entry(&fsh, 1, &e), FSH_ERR_BOUNDS);

    fsh_open(&fsh, data, size);
    put_u32(data + 28, (uint32_t)size - 4);  /* entry header past the end */
    CHECK_EQ_INT(fsh_entry(&fsh, 1, &e), FSH_ERR_BOUNDS);
    free(data);
}

int main(void)
{
    test_open_and_find();
    test_bitmap_entry();
    test_palette();
    test_corrupt();
    return TEST_REPORT();
}
