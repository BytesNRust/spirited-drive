/* Palette matcher tests. The expected indices come from the original
 * (NFS_WIN.EXE 00473140 with an 8-bit surface) run under Unicorn by
 * `tools/flat_oracle.py palette-golden`; the random comparison is
 * `flat_oracle.py palette-check`. */
#include "game/gfx/palette_match.h"
#include "test.h"

static const uint32_t colours[8] = { 0x000000, 0xFFFFFF, 0x123456, 0x808080,
                                     0x0000FF, 0x00FF00, 0xFF0000, 0xFF402010 };

/* The golden palette: entry i is (i, 255 - i, i * 7 mod 256, 0); entries whose
 * index is a multiple of 3 are disabled. */
static uint8_t palette[PALETTE_MATCH_ENTRIES * PALETTE_MATCH_ENTRY_BYTES];
static uint8_t enabled[PALETTE_MATCH_ENTRIES];
static uint8_t none[PALETTE_MATCH_ENTRIES];
static uint8_t inverse[PALETTE_MATCH_INVERSE_SIZE];

static void make_tables(void)
{
    for (int i = 0; i < PALETTE_MATCH_ENTRIES; i++) {
        palette[4 * i] = (uint8_t)i;
        palette[4 * i + 1] = (uint8_t)(255 - i);
        palette[4 * i + 2] = (uint8_t)(i * 7);
        enabled[i] = i % 3 != 0;
    }
    for (int i = 0; i < PALETTE_MATCH_INVERSE_SIZE; i++) inverse[i] = (uint8_t)(i * 5 + 3);
}

static void test_search(void)
{
    static const int expect[8] = { 110, 145, 149, 128, 221, 1, 145, 119 };
    const PaletteMatch m = { NULL, palette, enabled };
    for (int i = 0; i < 8; i++) CHECK_EQ_INT(palette_match_rgb(&m, colours[i]), expect[i]);
}

static void test_nothing_enabled(void)
{
    const PaletteMatch m = { NULL, palette, none };
    for (int i = 0; i < 8; i++) CHECK_EQ_INT(palette_match_rgb(&m, colours[i]), 0);
}

static void test_inverse(void)
{
    static const int expect[8] = { 3, 254, 245, 83, 158, 99, 3, 141 };
    const PaletteMatch m = { inverse, palette, enabled };
    for (int i = 0; i < 8; i++) CHECK_EQ_INT(palette_match_rgb(&m, colours[i]), expect[i]);
    /* bits 24-31 do not matter, and the low 3 bits of each channel are dropped */
    CHECK_EQ_INT(palette_match_rgb(&m, 0xFF123456), palette_match_rgb(&m, 0x123456));
    CHECK_EQ_INT(palette_match_rgb(&m, 0x000007), palette_match_rgb(&m, 0x000000));
}

/* The lowest index wins a tie, and a colour 99999 or more away from every
 * enabled entry gives 0 rather than the closest. */
static void test_ties_and_limit(void)
{
    uint8_t pal[PALETTE_MATCH_ENTRIES * PALETTE_MATCH_ENTRY_BYTES] = { 0 };
    uint8_t on[PALETTE_MATCH_ENTRIES] = { 0 };
    for (int i = 10; i < 20; i++) {
        pal[4 * i] = 100;
        on[i] = 1;
    }
    CHECK_EQ_INT(palette_match_nearest(pal, on, 100), 10);
    CHECK_EQ_INT(palette_match_nearest(pal, on, 101), 10);
    on[5] = 1;
    pal[4 * 5] = 90;
    CHECK_EQ_INT(palette_match_nearest(pal, on, 95), 5);  /* 25 away from both */
    for (int i = 0; i < PALETTE_MATCH_ENTRIES; i++) {
        pal[4 * i] = 0;
        on[i] = i == 77;
    }
    CHECK_EQ_INT(palette_match_nearest(pal, on, 0xFFFFFF), 0);  /* 195075 away: too far */
    CHECK_EQ_INT(palette_match_nearest(pal, on, 0x0000FF), 77);  /* 65025 away */

}

int main(void)
{
    make_tables();
    test_search();
    test_nothing_enabled();
    test_inverse();
    test_ties_and_limit();
    return TEST_REPORT();
}
