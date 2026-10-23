/* ICO tests: a 4-bit icon with its palette and AND mask comes out as RGBA, top row first; damage is refused. */
#include "formats/ico.h"
#include "test.h"

#include <stdint.h>
#include <string.h>

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v & 0xffff), put16(p + 2, v >> 16); }

/* a 2 x 2 icon, 4 bits a pixel: bottom row colours 1, 2; top row 3 and 0, the top-right pixel transparent */
static size_t icon(uint8_t *f)
{
    enum { DIB = 40, PALETTE = 16 * 4, ROWS = 2 * 4, MASK = 2 * 4 };
    memset(f, 0, 6 + 16 + DIB + PALETTE + ROWS + MASK);
    put16(f + 2, 1), put16(f + 4, 1);
    f[6] = 2, f[7] = 2, f[8] = 16;
    put16(f + 10, 1), put16(f + 12, 4), put32(f + 14, DIB + PALETTE + ROWS + MASK), put32(f + 18, 22);
    uint8_t *const b = f + 22;
    put32(b, DIB), put32(b + 4, 2), put32(b + 8, 4), put16(b + 12, 1), put16(b + 14, 4);
    uint8_t *const pal = b + DIB;   /* B, G, R, 0 */
    pal[4 * 1 + 2] = 0xff;          /* 1 red */
    pal[4 * 2 + 1] = 0xff;          /* 2 green */
    pal[4 * 3 + 0] = 0xff;          /* 3 blue */
    uint8_t *const rows = pal + PALETTE;
    rows[0] = 0x12;                 /* bottom row: 1, 2 */
    rows[4] = 0x30;                 /* top row: 3, 0 */
    uint8_t *const mask = rows + ROWS;
    mask[4] = 0x40;                 /* top row: the second pixel masked out */
    return 6 + 16 + DIB + PALETTE + ROWS + MASK;
}

static void test_a_four_bit_icon(void)
{
    uint8_t f[256], rgba[16];
    const size_t n = icon(f);
    int w = 0, h = 0;
    CHECK_EQ_INT(ico_decode(f, n, rgba, sizeof rgba, &w, &h), 1);
    CHECK_EQ_INT(w, 2);
    CHECK_EQ_INT(h, 2);
    const uint8_t want[16] = { 0, 0, 0xff, 0xff, 0, 0, 0, 0, 0xff, 0, 0, 0xff, 0, 0xff, 0, 0xff };
    CHECK(memcmp(rgba, want, sizeof want) == 0);   /* top: blue, transparent; bottom: red, green */
}

static void test_damage_is_refused(void)
{
    uint8_t f[256], rgba[16];
    const size_t n = icon(f);
    int w, h;
    CHECK_EQ_INT(ico_decode(f, 30, rgba, sizeof rgba, &w, &h), 0);       /* cut short */
    CHECK_EQ_INT(ico_decode(f, n, rgba, 8, &w, &h), 0);                  /* no room */
    f[2] = 2;
    CHECK_EQ_INT(ico_decode(f, n, rgba, sizeof rgba, &w, &h), 0);        /* a cursor, not an icon */
}

int main(void)
{
    test_a_four_bit_icon();
    test_damage_is_refused();
    return TEST_REPORT();
}
