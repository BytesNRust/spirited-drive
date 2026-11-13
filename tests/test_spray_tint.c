/* The spray's tint table (spray_tint.h): each colour moved toward the tint by the share asked. */
#include "game/race/spray_tint.h"
#include "tests/test.h"

#include <string.h>

static int luma(const uint8_t c[3]) { return (c[0] * 77 + c[1] * 150 + c[2] * 29) >> 8; }

static void put(uint8_t *pal, int i, int r, int g, int b) { pal[i * 3] = (uint8_t)r, pal[i * 3 + 1] = (uint8_t)g, pal[i * 3 + 2] = (uint8_t)b; }

static void test_the_table_moves_each_colour_toward_the_tint(void)
{
    uint8_t pal[768] = { 0 }, table[256];
    put(pal, 1, 250, 250, 250), put(pal, 2, 128, 128, 128), put(pal, 3, 80, 55, 30), put(pal, 4, 20, 20, 20);
    const uint8_t tint[3] = { 80, 55, 30 };
    spray_tint_table(pal, tint, 60, table);
    CHECK_EQ_INT(table[255], 255);                  /* the transparent index is left alone */
    CHECK_EQ_INT(table[3], 3);                      /* the tint itself is as it is */
    CHECK(table[1] != 1);                           /* a white is carried toward the brown... */
    CHECK(luma(&pal[table[1] * 3]) < 250);          /* ...and so is darker */
    spray_tint_table(pal, tint, 0, table);
    CHECK_EQ_INT(table[1], 1);                      /* no share: the colour itself */
    CHECK_EQ_INT(table[2], 2);
    spray_tint_table(pal, tint, 100, table);
    CHECK_EQ_INT(table[1], 3);                      /* all of it: the tint's entry */
    CHECK_EQ_INT(table[4], 3);
}

int main(void)
{
    test_the_table_moves_each_colour_toward_the_tint();
    return TEST_REPORT();
}
