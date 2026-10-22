#include "formats/ffn.h"

#include <string.h>

enum {
    HEADER_SIZE = 0x20,
    RECORD_SIZE = 11,
    ATLAS_HEADER = 16,
    ATLAS_TYPE_4BPP = 0x7A
};

static uint16_t u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void decode(const uint8_t *r, FfnGlyph *g)
{
    g->code = u16(r);
    g->width = r[2];
    g->height = r[3];
    g->atlas_x = u16(r + 4);
    g->atlas_y = u16(r + 6);
    g->advance = r[8];
    g->x_offset = (int8_t)r[9];
    g->y_offset = (int8_t)r[10];
}

static int glyphs_fit_atlas(const Ffn *font)
{
    for (uint16_t i = 0; i < font->glyph_count; i++) {
        FfnGlyph g;
        decode(font->records + (size_t)i * RECORD_SIZE, &g);
        if ((uint32_t)g.atlas_x + g.width > font->atlas_width || (uint32_t)g.atlas_y + g.height > font->atlas_height) {
            return 0;
        }
    }
    return 1;
}

FfnResult ffn_open(Ffn *font, const uint8_t *data, size_t size)
{
    if (size < HEADER_SIZE || memcmp(data, "FNTF", 4) != 0) return FFN_ERR_HEADER;
    Ffn f;
    f.data = data;
    f.glyph_count = u16(data + 0x0A);
    f.line_height = u16(data + 0x12);
    const uint32_t atlas_at = u32(data + 0x1C);
    if (HEADER_SIZE + (size_t)f.glyph_count * RECORD_SIZE > atlas_at || atlas_at > size || size - atlas_at < ATLAS_HEADER) {
        return FFN_ERR_BOUNDS;
    }
    const uint8_t *atlas = data + atlas_at;
    if (atlas[0] != ATLAS_TYPE_4BPP) return FFN_ERR_HEADER;
    f.records = data + HEADER_SIZE;
    f.atlas_width = u16(atlas + 4);
    f.atlas_height = u16(atlas + 6);
    f.atlas = atlas + ATLAS_HEADER;
    const size_t pixel_bytes = ((size_t)f.atlas_width * f.atlas_height + 1) / 2;
    if (pixel_bytes > size - atlas_at - ATLAS_HEADER || !glyphs_fit_atlas(&f)) return FFN_ERR_BOUNDS;
    *font = f;
    return FFN_OK;
}

int ffn_glyph(const Ffn *font, unsigned code, FfnGlyph *out)
{
    for (uint16_t i = 0; i < font->glyph_count; i++) {
        const uint8_t *r = font->records + (size_t)i * RECORD_SIZE;
        if (u16(r) == code) {
            decode(r, out);
            return 1;
        }
    }
    return 0;
}

int ffn_glyph_level(const Ffn *font, const FfnGlyph *glyph, int x, int y)
{
    const size_t pixel = (size_t)(glyph->atlas_y + y) * font->atlas_width + glyph->atlas_x + x;
    const uint8_t b = font->atlas[pixel / 2];
    return (pixel & 1) ? (b & 0x0F) : (b >> 4);
}

int ffn_text_width(const Ffn *font, const char *text)
{
    int width = 0;
    for (const unsigned char *c = (const unsigned char *)text; *c; c++) {
        FfnGlyph g;
        if (ffn_glyph(font, *c, &g)) width += g.advance;
    }
    return width;
}

int ffn_scaled(int length, int num, int den)
{
    return (length * num + den - 1) / den;
}

/* the source position of a scaled pixel's centre in 1/256ths, clamped to [0, size - 1] */
static int source_at(int scaled, int num, int den, int size)
{
    int s = ((2 * scaled + 1) * den * 256 / num - 256) / 2;
    return s < 0 ? 0 : s > (size - 1) * 256 ? (size - 1) * 256 : s;
}

int ffn_glyph_level_scaled(const Ffn *font, const FfnGlyph *glyph, int x, int y, int num, int den)
{
    const int sx = source_at(x, num, den, glyph->width), sy = source_at(y, num, den, glyph->height);
    const int x0 = sx >> 8, y0 = sy >> 8, fx = sx & 0xff, fy = sy & 0xff;
    const int x1 = x0 + 1 < glyph->width ? x0 + 1 : x0, y1 = y0 + 1 < glyph->height ? y0 + 1 : y0;
    const int top = ffn_glyph_level(font, glyph, x0, y0) * (256 - fx) + ffn_glyph_level(font, glyph, x1, y0) * fx;
    const int bottom = ffn_glyph_level(font, glyph, x0, y1) * (256 - fx) + ffn_glyph_level(font, glyph, x1, y1) * fx;
    return (top * (256 - fy) + bottom * fy + (1 << 15)) >> 16;
}
