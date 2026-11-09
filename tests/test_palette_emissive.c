/* The emissive entries (palette.h palette_emissive_set): no tint, light table or fog table moves them. */
#include "tests/test.h"

#include "game/race/psx_lgs.h"
#include "game/sys/palette.h"
#include "viewer/modern_road.h"

#include <string.h>

enum { LAMP = 0x47 };

static uint8_t pal[768];

static void make_palette(void)
{
    for (int i = 0; i < 256; i++) pal[3 * i] = (uint8_t)i, pal[3 * i + 1] = (uint8_t)(255 - i), pal[3 * i + 2] = (uint8_t)(i / 2);
    pal[3 * LAMP] = 251, pal[3 * LAMP + 1] = 20, pal[3 * LAMP + 2] = 20;
}

static void test_tint_keeps_the_entry(void)
{
    const float tint[3] = { 0.875f, 0.8f, 0.7f };
    uint8_t out[768];
    palette_emissive_set(NULL);
    psx_tint_palette(pal, tint, out);
    CHECK(out[3 * LAMP] != 251);   /* tinted as any */
    uint8_t exempt[256] = { 0 };
    exempt[LAMP] = 1;
    palette_emissive_set(exempt);
    psx_tint_palette(pal, tint, out);
    CHECK_EQ_INT(out[3 * LAMP], 251);
    CHECK_EQ_INT(out[3 * LAMP + 1], 20);
    CHECK_EQ_INT(out[3 * LAMP + 2], 20);
    CHECK(out[3 * 0x48] != pal[3 * 0x48]);   /* the others are still tinted */
    palette_emissive_set(NULL);
}

static void test_lit_table_keeps_the_entry(void)
{
    const float ratio[3] = { 0.5f, 0.5f, 0.5f };
    uint8_t table[256];
    palette_emissive_set(NULL);
    psx_lit_table(pal, ratio, table);
    CHECK(table[LAMP] != LAMP);
    uint8_t exempt[256] = { 0 };
    exempt[LAMP] = 1;
    palette_emissive_set(exempt);
    psx_lit_table(pal, ratio, table);
    CHECK_EQ_INT(table[LAMP], LAMP);
    CHECK(table[0x20] != 0x20);
    palette_emissive_set(NULL);
}

/* port (car_tunnel_match): the any-family table keeps what the table keeps (the emissive entry, the shadow and see-through texels), and with the family on it is psx_lit_table */
static void test_lit_table_any_family(void)
{
    const float ratio[3] = { 1.6f, 1.6f, 1.6f };
    uint8_t plain[256], family[256], any[256], exempt[256] = { 0 };
    exempt[LAMP] = 1;
    palette_emissive_set(exempt);
    psx_lit_table(pal, ratio, plain);
    psx_lit_table_ex(pal, ratio, 0, family);
    psx_lit_table_ex(pal, ratio, 1, any);
    CHECK(memcmp(plain, family, sizeof plain) == 0);
    CHECK_EQ_INT(any[LAMP], LAMP);
    CHECK_EQ_INT(any[0xfe], 0xfe);
    CHECK_EQ_INT(any[0xff], 0xff);
    palette_emissive_set(NULL);
}

static void test_fog_and_light_tables_keep_the_entry(void)
{
    const uint8_t fog[3] = { 128, 128, 128 };
    const float tint[3] = { 0.5f, 0.5f, 0.5f };
    static uint8_t tables[MODERN_FOG_LEVELS][256];
    uint8_t exempt[256] = { 0 };
    exempt[LAMP] = 1;
    palette_emissive_set(NULL);
    modern_fog_tables(pal, fog, tables);
    CHECK(tables[MODERN_FOG_LEVELS - 1][LAMP] != LAMP);
    palette_emissive_set(exempt);
    modern_fog_tables(pal, fog, tables);
    for (int l = 0; l < MODERN_FOG_LEVELS; l++) CHECK_EQ_INT(tables[l][LAMP], LAMP);
    CHECK(tables[MODERN_FOG_LEVELS - 1][0x20] != 0x20);
    modern_light_tables(pal, tint, tables);
    for (int l = 0; l < MODERN_FOG_LEVELS; l++) CHECK_EQ_INT(tables[l][LAMP], LAMP);
    CHECK(tables[MODERN_FOG_LEVELS - 1][0x20] != 0x20);
    palette_emissive_set(NULL);
}

int main(void)
{
    make_palette();
    test_tint_keeps_the_entry();
    test_lit_table_keeps_the_entry();
    test_lit_table_any_family();
    test_fog_and_light_tables_keep_the_entry();
    return TEST_REPORT();
}
