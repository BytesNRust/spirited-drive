#include "game/sys/text.h"

#include "formats/ffn.h"
#include "game/sys/palette.h"
#include "game/sys/resource.h"
#include "game/sys/video.h"

#include <stdio.h>
#include <string.h>

enum { STYLE_COUNT = 4 };

static uint8_t *font_data;   /* 00524824 */
static Ffn font;
static uint8_t styles[STYLE_COUNT][16];
static uint8_t shadow[16];   /* 0051fd98 */
static uint8_t current[16];  /* 0051dd18 */

int text_init(void)
{
    size_t size;
    font_data = resource_load_art("fonts/graver18.ffn", &size);
    if (font_data == NULL || ffn_open(&font, font_data, size) != FFN_OK) {
        fprintf(stderr, "text: cannot load fonts/graver18.ffn\n");
        text_free();
        return 0;
    }
    return 1;
}

void text_free(void)
{
    resource_free(font_data);
    font_data = NULL;
}

static void build_ramp(const uint8_t *pal, const uint8_t *usable, int r, int g, int b, uint8_t ramp[16])
{
    const int fg = palette_nearest(pal, usable, r, g, b);  /* 00473140 */
    palette_text_ramp(pal, usable, fg, 0, ramp);            /* 0048f370(colour, 0) */
}

void text_build_styles(void)
{
    uint8_t pal[PALETTE_BYTES];
    video_get_palette(pal);
    text_build_styles_from(pal);
}

void text_build_styles_from(const uint8_t pal[768])
{
    const uint8_t *usable = palette_usable();  /* 004c5ff8, as the last race start left it */
    build_ramp(pal, usable, 0xFF, 0xFF, 0xFF, styles[TEXT_WHITE]);
    build_ramp(pal, usable, 0xE8, 0xD3, 0xB7, styles[TEXT_CREAM]);
    build_ramp(pal, usable, 0xFF, 0xCE, 0x06, styles[TEXT_YELLOW]);
    build_ramp(pal, usable, 0xFF, 0x06, 0x06, styles[TEXT_RED]);
    build_ramp(pal, usable, 0xFF, 0xCE, 0x06, current);
    build_ramp(pal, usable, 0x00, 0x00, 0x00, shadow);
}

void text_select(TextStyle style)
{
    memcpy(current, styles[style < STYLE_COUNT ? style : TEXT_CREAM], sizeof current);
}

int text_width(const char *text)
{
    return font_data != NULL ? ffn_text_width(&font, text) : 0;
}

int text_line_height(void)
{
    return font_data != NULL ? font.line_height : 0;
}

static void draw_glyph(const FfnGlyph *g, int pen_x, int pen_y, const uint8_t ramp[16])
{
    int tw, th;
    uint8_t *const to = video_target(&tw, &th);
    for (int y = 0; y < g->height; y++) {
        const int dy = pen_y + g->y_offset + y;
        if (dy < 0 || dy >= th) continue;
        for (int x = 0; x < g->width; x++) {
            const int dx = pen_x + g->x_offset + x;
            const int level = ffn_glyph_level(&font, g, x, y);
            if (level != 0 && dx >= 0 && dx < tw) to[dy * tw + dx] = ramp[level];
        }
    }
}

/* 0048c0a8: glyphs >= 0x21 advance by their own width, other codes by the
 * space's, '\n' moves down a line and back to x = 0, NUL ends. */
static void draw_with(const char *text, int x, int y, const uint8_t ramp[16])
{
    if (font_data == NULL) return;
    FfnGlyph space = { 0 };
    ffn_glyph(&font, ' ', &space);
    int pen_x = x, pen_y = y;
    for (const unsigned char *c = (const unsigned char *)text; *c; c++) {
        FfnGlyph g;
        if (*c == '\n') {
            pen_x = 0;
            pen_y += font.line_height;
        } else if (*c < 0x21) {
            pen_x += space.advance;
        } else if (ffn_glyph(&font, *c, &g)) {
            draw_glyph(&g, pen_x, pen_y, ramp);
            pen_x += g.advance;
        }
    }
}

static const uint8_t *drawn_ramp = current;  /* 0048ec60's choice for 0048c264 */

void text_ramp(int use_shadow)
{
    drawn_ramp = use_shadow ? shadow : current;
}

void text_draw(const char *text, int x, int y)
{
    draw_with(text, x, y, drawn_ramp);
}

void text_draw_shadowed(const char *text, int x, int y)
{
    draw_with(text, x + 2, y + 2, shadow);
    draw_with(text, x, y, current);
}

void text_draw_centered(const char *text, int cx, int y)
{
    text_draw_shadowed(text, cx - (short)text_width(text) / 2, y);
}
