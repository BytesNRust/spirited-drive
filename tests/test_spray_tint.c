/* The rally's spray colour (spray_tint.h): the colour the road gives it and the table that paints with it. */
#include "game/race/spray_tint.h"
#include "tests/test.h"

#include <string.h>

static int luma(const uint8_t c[3]) { return (c[0] * 77 + c[1] * 150 + c[2] * 29) >> 8; }

static void test_the_spray_is_a_darker_road_and_the_snows_stays_white(void)
{
    const uint8_t gravel[3] = { 110, 105, 100 }, dirt[3] = { 130, 95, 60 }, snow[3] = { 225, 230, 238 };
    uint8_t t[3];
    spray_tint_target(gravel, t);
    CHECK(luma(t) < luma(gravel) && t[0] >= t[1] && t[1] >= t[2]);   /* greyer-darker, still grey */
    spray_tint_target(dirt, t);
    CHECK(luma(t) < luma(dirt) && t[0] > t[1] && t[1] > t[2]);        /* darker, still brown */
    spray_tint_target(snow, t);
    CHECK(luma(t) >= 210);                                            /* snow throws white powder */
}

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

static void test_a_key_groups_alike_colours(void)
{
    const uint8_t a[3] = { 100, 90, 80 }, b[3] = { 101, 91, 81 }, c[3] = { 140, 90, 80 };
    CHECK_EQ_INT(spray_tint_key(a), spray_tint_key(b));
    CHECK(spray_tint_key(a) != spray_tint_key(c));
}

/* port (spray_cd_smoke): the mist the CD's smoke sprite is drawn with: the colour by surface, the additive tables, the sprite's grey as levels */
static void test_the_mist_colour_follows_the_surface(void)
{
    const uint8_t asphalt[3] = { 98, 100, 104 }, dirt[3] = { 134, 113, 89 }, snow[3] = { 225, 230, 238 };
    uint8_t c[3];
    CHECK_EQ_INT(spray_mist_kind(asphalt), SPRAY_MIST_WATER);
    CHECK_EQ_INT(spray_mist_kind(dirt), SPRAY_MIST_MUD);
    CHECK_EQ_INT(spray_mist_kind(snow), SPRAY_MIST_SNOW);
    spray_mist_colour(asphalt, c);
    CHECK(luma(c) >= 200 && c[2] >= c[0]);          /* water: white, a touch cool */
    spray_mist_colour(dirt, c);
    CHECK(luma(c) > luma(dirt) && c[0] > c[1] && c[1] > c[2]);   /* dust: the road's own hue, lighter: beige over brown */
    spray_mist_colour(snow, c);
    CHECK(luma(c) >= 230);
}

static void test_the_additive_table_lifts_each_colour_by_the_amount(void)
{
    uint8_t pal[768] = { 0 }, table[256];
    put(pal, 1, 20, 20, 20), put(pal, 2, 70, 70, 70), put(pal, 3, 130, 130, 130), put(pal, 4, 200, 200, 200), put(pal, 5, 250, 250, 250);
    const uint8_t white[3] = { 250, 250, 250 };
    spray_add_table(pal, white, 0, table);
    CHECK_EQ_INT(table[1], 1); CHECK_EQ_INT(table[3], 3);      /* no amount: the colour itself */
    spray_add_table(pal, white, 255, table);
    CHECK_EQ_INT(table[255], 255);                              /* the transparent index is left alone */
    CHECK_EQ_INT(table[1], 5); CHECK_EQ_INT(table[4], 5);      /* all of it: everything goes to the brightest */
    spray_add_table(pal, white, 60, table);
    CHECK(luma(&pal[table[1] * 3]) > 20 && luma(&pal[table[1] * 3]) < 120);   /* a little: a little lighter */
    CHECK(table[2] >= table[1] && table[3] >= table[2]);        /* and never the other way round */
}

static void test_the_sprites_grey_becomes_levels(void)
{
    uint8_t shape[16 + 4 * 2] = { 0x7b, 0, 0, 0, 4, 0, 2, 0 }, grey[256] = { 0 }, out[16 + 4 * 2];
    grey[10] = 0, grey[11] = 40, grey[12] = 128, grey[13] = 255;
    shape[16] = 10, shape[17] = 11, shape[18] = 12, shape[19] = 13, shape[20] = 0xFF, shape[21] = 13, shape[22] = 11, shape[23] = 10;
    CHECK_EQ_INT(spray_mist_shape(shape, grey, out, sizeof out), 1);
    CHECK_EQ_INT(out[0], 0x7b); CHECK_EQ_INT(out[4], 4); CHECK_EQ_INT(out[6], 2);   /* the header as it was */
    CHECK_EQ_INT(out[16], 0);                                   /* a black texel is nothing */
    CHECK(out[17] >= 1 && out[18] > out[17] && out[19] == SPRAY_MIST_LEVELS);   /* brighter, more level, the whitest the top */
    CHECK_EQ_INT(out[20], 0);                                   /* the transparent index is nothing */
    uint8_t small[8];
    CHECK_EQ_INT(spray_mist_shape(shape, grey, small, sizeof small), 0);   /* no room: refused */
    shape[0] = 0x7a;
    CHECK_EQ_INT(spray_mist_shape(shape, grey, out, sizeof out), 0);        /* not an 8-bit shape */
}

int main(void)
{
    test_the_mist_colour_follows_the_surface();
    test_the_additive_table_lifts_each_colour_by_the_amount();
    test_the_sprites_grey_becomes_levels();
    test_the_spray_is_a_darker_road_and_the_snows_stays_white();
    test_the_table_moves_each_colour_toward_the_tint();
    test_a_key_groups_alike_colours();
    return TEST_REPORT();
}
