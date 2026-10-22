/* FFN: EA "FNTF" bitmap fonts (frontend/art/fonts/graver18.ffn and others).
 *
 *   0x00 "FNTF"  0x04 u32 file size  0x08 u16 version (100)  0x0A u16 glyphs
 *   0x12 u16 line height  0x14 u16 first code  0x1C u32 atlas offset
 *   0x20 glyphs x 11 bytes: u16 code, u8 w, u8 h, u16 x, u16 y (in the atlas),
 *        u8 advance, i8 x offset, i8 y offset (from the top of the line)
 *   atlas: FSH-style entry of type 0x7A, 4 bits per pixel (high nibble first),
 *          each value an anti-aliasing level 0 (empty) .. 15 (solid).
 * The game also accepts FNT3/FNTP/FNTS/FNTU ("setfont" message); the CD only
 * ships FNTF. */
#ifndef TNFS_FFN_H
#define TNFS_FFN_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    FFN_OK = 0,
    FFN_ERR_HEADER,  /* not an FNTF font */
    FFN_ERR_BOUNDS   /* glyph table or atlas outside the data */
} FfnResult;

typedef struct {
    const uint8_t *data;  /* kept, must outlive the Ffn */
    uint16_t glyph_count;
    uint16_t line_height;
    const uint8_t *records;
    const uint8_t *atlas;
    uint16_t atlas_width;
    uint16_t atlas_height;
} Ffn;

typedef struct {
    uint16_t code;
    uint8_t width, height;
    uint16_t atlas_x, atlas_y;
    uint8_t advance;
    int8_t x_offset, y_offset;
} FfnGlyph;

FfnResult ffn_open(Ffn *font, const uint8_t *data, size_t size);
int ffn_glyph(const Ffn *font, unsigned code, FfnGlyph *out);
int ffn_glyph_level(const Ffn *font, const FfnGlyph *glyph, int x, int y);

/* port: the level at (x, y) of the glyph drawn num/den times its size (bilinear between atlas pixels, clamped to the
 * glyph), and a length at that size rounded up */
int ffn_glyph_level_scaled(const Ffn *font, const FfnGlyph *glyph, int x, int y, int num, int den);
int ffn_scaled(int length, int num, int den);
/* how much of the scaled pixel (x, y), 0..15, lies where the bilinear levels reach `threshold`: 4x4 samples, so
 * the scaled glyph's outline is the smooth contour of its levels, anti-aliased by area */
int ffn_glyph_coverage_scaled(const Ffn *font, const FfnGlyph *glyph, int x, int y, int num, int den, int threshold);

/* 004770f4: sum of advances; a space uses the 0x20 glyph's advance. */
int ffn_text_width(const Ffn *font, const char *text);

#endif
