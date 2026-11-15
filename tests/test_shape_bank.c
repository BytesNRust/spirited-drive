/* A shape bank checked before the game searches it (shape_bank.h): a mod's art bank whose directory, offsets or shapes run past
 * its bytes is refused where it is loaded, and the search never reads past a bank that passed */
#include "tests/test.h"

#include "game/sys/shape_bank.h"
#include "game/viewer/drive_livery.h"

#include <stdlib.h>

#include <string.h>

enum { HEADER = 16, ENTRY = 8, SHAPE_HEADER = 16, PALETTE = 768 };

static uint8_t bank[4096];

/* the livery's file calls (no car masks or colours here) */
uint8_t *platform_read_file(const char *path, size_t *size) { (void)path, (void)size; return NULL; }
void platform_free_file(uint8_t *data) { free(data); }

static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v), put16(p + 2, v >> 16); }

/* an SHPI of `count` entries, its directory then the shapes from `first` */
static void shpi(uint32_t size, uint32_t count)
{
    memset(bank, 0, sizeof bank);
    memcpy(bank, "SHPI", 4), put32(bank + 4, size), put32(bank + 8, count), memcpy(bank + 12, "GIMX", 4);
}

static void entry(uint32_t n, const char name[4], uint32_t at, uint8_t type, uint16_t w, uint16_t h)
{
    memcpy(bank + HEADER + ENTRY * n, name, 4), put32(bank + HEADER + ENTRY * n + 4, at);
    bank[at] = type, put16(bank + at + 4, w), put16(bank + at + 6, h);
}

/* a bank as the CD's: a bitmap and its palette, each within the bytes */
static size_t good_bank(void)
{
    const uint32_t bmp = HEADER + 2 * ENTRY, pal = bmp + SHAPE_HEADER + 8 * 4;
    const uint32_t size = pal + SHAPE_HEADER + PALETTE;
    shpi(size, 2);
    entry(0, "tach", bmp, 0x7b, 8, 4);
    entry(1, "!pal", pal, 0x22, 256, 3);
    return size;
}

static void test_good_bank(void)
{
    const size_t size = good_bank();
    CHECK(shape_bank_valid(bank, size));
    CHECK(shape_bank_find(bank, (const uint8_t *)"tach") == bank + HEADER + 2 * ENTRY);
    CHECK(shape_bank_find(bank, (const uint8_t *)"!pal") != NULL);
    CHECK(shape_bank_find(bank, (const uint8_t *)"none") == NULL);
}

/* not a bank, or one too short for its header or directory */
static void test_header(void)
{
    CHECK(!shape_bank_valid(NULL, 0));
    CHECK(!shape_bank_valid(bank, 8));
    const size_t size = good_bank();
    memcpy(bank, "SHPX", 4);
    CHECK(!shape_bank_valid(bank, size));
    good_bank();
    put32(bank + 8, 400);   /* 400 entries: the directory runs past the bytes */
    CHECK(!shape_bank_valid(bank, size));
    good_bank();
    put32(bank + 8, 0x80000000u);   /* read as a negative count by the search */
    CHECK(!shape_bank_valid(bank, size));
}

/* a shape whose offset or header runs past the bank */
static void test_offsets(void)
{
    const size_t size = good_bank();
    put32(bank + HEADER + 4, (uint32_t)size + 100);
    CHECK(!shape_bank_valid(bank, size));
    good_bank();
    put32(bank + HEADER + 4, (uint32_t)size - 4);   /* its 16-byte header does not fit */
    CHECK(!shape_bank_valid(bank, size));
}

/* a bitmap whose pixels run past the bank */
static void test_bitmap_pixels(void)
{
    const size_t size = good_bank();
    put16(bank + HEADER + 2 * ENTRY + 4, 4000), put16(bank + HEADER + 2 * ENTRY + 6, 4000);
    CHECK(!shape_bank_valid(bank, size));
}

/* a palette, or a shape named "!pal" (read as one), with fewer than 768 bytes; a shape of a type the game does not draw */
static void test_palettes_and_types(void)
{
    size_t size = good_bank();
    CHECK(!shape_bank_valid(bank, size - 1));   /* the palette is the last shape: one byte short */
    size = good_bank();
    bank[HEADER + 2 * ENTRY] = 0x7d;
    CHECK(!shape_bank_valid(bank, size));
    size = good_bank();
    memcpy(bank + HEADER + ENTRY, "tach", 4);   /* a palette under a bitmap's name: blitted, it would read its 256 x 3 */
    CHECK(!shape_bank_valid(bank, size));
    size = good_bank();
    entry(1, "!pal", HEADER + 2 * ENTRY + SHAPE_HEADER + 8 * 4, 0x7b, 1, 1);   /* a bitmap named as a palette */
    CHECK(!shape_bank_valid(bank, size - PALETTE + 4));
    entry(1, "!xxx", HEADER + 2 * ENTRY + SHAPE_HEADER + 8 * 4, 0x7b, 256, 1);   /* another '!' bitmap, as the CD's cars' */
    CHECK(shape_bank_valid(bank, size - PALETTE + 256));
}

/* the dash paint (drive_livery_dash) on a bank whose directory says 1000 shapes in 32 bytes: refused, not searched past its end;
 * and on a bank whose 'dash' says its palette follows 16 MB on: refused, not read there */
static void test_dash_paint_refuses(void)
{
    static const uint8_t rgb[3] = { 200, 0, 0 };
    uint8_t *small = calloc(1, 32);
    CHECK(small != NULL);
    if (small == NULL) return;
    memcpy(small, "SHPI", 4), put32(small + 4, 32), put32(small + 8, 1000), memcpy(small + 12, "GIMX", 4);
    CHECK_EQ_INT(drive_livery_dash("P911", small, 32, rgb), 0);
    free(small);
    const size_t size = good_bank();
    memcpy(bank + HEADER + ENTRY, "dash", 4);   /* no '!pal': the palette after 'dash''s block */
    bank[HEADER + 2 * ENTRY + 1] = 0xff, bank[HEADER + 2 * ENTRY + 2] = 0xff, bank[HEADER + 2 * ENTRY + 3] = 0xff;
    memcpy(bank + HEADER, "dash", 4), memcpy(bank + HEADER + ENTRY, "tach", 4);
    CHECK_EQ_INT(drive_livery_dash("P911", bank, size, rgb), 0);
}

int main(void)
{
    test_dash_paint_refuses();
    test_good_bank();
    test_header();
    test_offsets();
    test_bitmap_pixels();
    test_palettes_and_types();
    return TEST_REPORT();
}
