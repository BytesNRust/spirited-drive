/* FNTF font tests. Layout read from the CD's fonts and the game's text code
 * (string drawing 0048c0a8, width 004770f4). */
#include "formats/ffn.h"
#include "test.h"

#include <stdlib.h>

enum { RECORDS_AT = 0x20, RECORD_SIZE = 11 };

/* Two glyphs: ' ' (advance 5) and 'A' 3x2 at atlas (1,0), advance 4, offsets (1,6).
 * Atlas 4x2 at 4 bits per pixel, high nibble first. */
static uint8_t *build_font(size_t *size)
{
    const size_t atlas_at = RECORDS_AT + 2 * RECORD_SIZE;
    *size = atlas_at + 16 + 4;
    uint8_t *f = calloc(1, *size);
    if (f == NULL) return NULL;
    memcpy(f, "FNTF", 4);
    f[4] = (uint8_t)*size;
    f[8] = 100;                 /* version */
    f[0x0A] = 2;                /* glyph count */
    f[0x12] = 20;               /* line height */
    f[0x14] = 0x20;             /* first code */
    f[0x1C] = (uint8_t)atlas_at;

    const uint8_t space[RECORD_SIZE] = { 0x20, 0, 1, 1, 0, 0, 0, 0, 5, 0, 16 };
    const uint8_t a[RECORD_SIZE] = { 'A', 0, 3, 2, 1, 0, 0, 0, 4, 1, 6 };
    memcpy(f + RECORDS_AT, space, RECORD_SIZE);
    memcpy(f + RECORDS_AT + RECORD_SIZE, a, RECORD_SIZE);

    uint8_t *atlas = f + atlas_at;
    atlas[0] = 0x7A;
    atlas[4] = 4;               /* width */
    atlas[6] = 2;               /* height */
    atlas[16] = 0x0F; atlas[17] = 0x8A;   /* row 0: 0 F 8 A */
    atlas[18] = 0x01; atlas[19] = 0x23;   /* row 1: 0 1 2 3 */
    return f;
}

static void test_open_and_glyphs(void)
{
    size_t size;
    uint8_t *data = build_font(&size);
    if (data == NULL) return;
    Ffn font;
    CHECK_EQ_INT(ffn_open(&font, data, size), FFN_OK);
    CHECK_EQ_INT(font.line_height, 20);

    FfnGlyph g;
    CHECK(ffn_glyph(&font, 'A', &g));
    CHECK_EQ_INT(g.width, 3);
    CHECK_EQ_INT(g.height, 2);
    CHECK_EQ_INT(g.advance, 4);
    CHECK_EQ_INT(g.x_offset, 1);
    CHECK_EQ_INT(g.y_offset, 6);
    CHECK_EQ_INT(ffn_glyph_level(&font, &g, 0, 0), 0xF);
    CHECK_EQ_INT(ffn_glyph_level(&font, &g, 2, 0), 0xA);
    CHECK_EQ_INT(ffn_glyph_level(&font, &g, 0, 1), 0x1);
    CHECK(!ffn_glyph(&font, 'B', &g));
    free(data);
}

static void test_text_width(void)
{
    size_t size;
    uint8_t *data = build_font(&size);
    if (data == NULL) return;
    Ffn font;
    ffn_open(&font, data, size);
    CHECK_EQ_INT(ffn_text_width(&font, "AA A"), 4 + 4 + 5 + 4);
    CHECK_EQ_INT(ffn_text_width(&font, ""), 0);
    CHECK_EQ_INT(ffn_text_width(&font, "AB"), 4);  /* missing glyphs take no space */
    free(data);
}

static void test_corrupt(void)
{
    size_t size;
    uint8_t *data = build_font(&size);
    if (data == NULL) return;
    Ffn font;
    CHECK_EQ_INT(ffn_open(&font, data, 10), FFN_ERR_HEADER);
    CHECK_EQ_INT(ffn_open(&font, data, size - 1), FFN_ERR_BOUNDS);  /* atlas pixels cut */

    data[RECORDS_AT + RECORD_SIZE + 4] = 3;  /* 'A' now reaches past the atlas width */
    CHECK_EQ_INT(ffn_open(&font, data, size), FFN_ERR_BOUNDS);
    data[RECORDS_AT + RECORD_SIZE + 4] = 1;

    data[0x0A] = 200;  /* records overlap the atlas */
    CHECK_EQ_INT(ffn_open(&font, data, size), FFN_ERR_BOUNDS);
    data[0x0A] = 2;

    memcpy(data, "FNT3", 4);
    CHECK_EQ_INT(ffn_open(&font, data, size), FFN_ERR_HEADER);  /* other variants unsupported */
    free(data);
}

int main(void)
{
    test_open_and_glyphs();
    test_text_width();
    test_corrupt();
    return TEST_REPORT();
}
