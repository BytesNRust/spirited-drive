#include "game/sys/port_log.h"
#include "game/sys/text.h"

#include "formats/ffn.h"
#include "game/sys/palette.h"
#include "game/sys/resource.h"
#include "game/sys/text_soft.h"
#include "game/sys/video.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { STYLE_COUNT = 5 };

static uint8_t *font_data;   /* 00524824 */
static Ffn font;
static uint8_t *small_data;  /* port: the CD's small face (FRONTEND/ART/MAIN18.FFN), loaded for the stats overlay alone */
static Ffn small;
static uint8_t styles[STYLE_COUNT][16];
static int styles_fg[STYLE_COUNT];   /* port: each style's own colour, which its ramp's level 15 only nears */
static uint8_t shadow[16];   /* 0051fd98 */
static uint8_t current[16];  /* 0051dd18 */
/* port: the palette the styles were built from, and the ramps from each colour under the text towards a text colour
 * (palette_text_ramp with the pixel below as level 0), built as met */
static uint8_t styles_pal[PALETTE_BYTES];
static uint8_t blend_ramps[256][256][16];
static uint8_t blend_known[256][256];

int text_init(void)
{
    size_t size;
    font_data = resource_load_art("fonts/graver18.ffn", &size);
    if (font_data == NULL || ffn_open(&font, font_data, size) != FFN_OK) {
        port_fatal("text: cannot load fonts/graver18.ffn\n");
        text_free();
        return 0;
    }
    return 1;
}

int text_loaded(void) { return font_data != NULL; }

int text_small_init(void)
{
    size_t size;
    if (small_data != NULL) return 1;
    small_data = resource_load_art("main18.ffn", &size);
    if (small_data == NULL || ffn_open(&small, small_data, size) != FFN_OK) {
        port_fatal("text: cannot load main18.ffn\n");
        resource_free(small_data);
        small_data = NULL;
        return 0;
    }
    return 1;
}

void text_free(void)
{
    resource_free(font_data);
    font_data = NULL;
    font.data = NULL;
    resource_free(small_data);
    small_data = NULL;
    small.data = NULL;
}

static int build_ramp(const uint8_t *pal, const uint8_t *usable, int r, int g, int b, uint8_t ramp[16])
{
    const int fg = palette_nearest(pal, usable, r, g, b);  /* 00473140 */
    palette_text_ramp(pal, usable, fg, 0, ramp);            /* 0048f370(colour, 0) */
    return fg;
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
    memcpy(styles_pal, pal, sizeof styles_pal);
    memset(blend_known, 0, sizeof blend_known);
    styles_fg[TEXT_WHITE] = build_ramp(pal, usable, 0xFF, 0xFF, 0xFF, styles[TEXT_WHITE]);
    styles_fg[TEXT_CREAM] = build_ramp(pal, usable, 0xE8, 0xD3, 0xB7, styles[TEXT_CREAM]);
    styles_fg[TEXT_YELLOW] = build_ramp(pal, usable, 0xFF, 0xCE, 0x06, styles[TEXT_YELLOW]);
    styles_fg[TEXT_RED] = build_ramp(pal, usable, 0xFF, 0x06, 0x06, styles[TEXT_RED]);
    styles_fg[TEXT_DIM] = build_ramp(pal, usable, 0x8C, 0x82, 0x73, styles[TEXT_DIM]);
    build_ramp(pal, usable, 0xFF, 0xCE, 0x06, current);
    build_ramp(pal, usable, 0x00, 0x00, 0x00, shadow);
}

static TextStyle current_style = TEXT_CREAM;   /* port: which style `current` holds, for text_draw_smooth */

void text_select(TextStyle style)
{
    current_style = style < STYLE_COUNT ? style : TEXT_CREAM;
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

static void draw_glyph(const Ffn *f, const FfnGlyph *g, int pen_x, int pen_y, const uint8_t ramp[16])
{
    int tw, th;
    uint8_t *const to = video_target(&tw, &th);
    for (int y = 0; y < g->height; y++) {
        const int dy = pen_y + g->y_offset + y;
        if (dy < 0 || dy >= th) continue;
        for (int x = 0; x < g->width; x++) {
            const int dx = pen_x + g->x_offset + x;
            const int level = ffn_glyph_level(f, g, x, y);
            if (level != 0 && dx >= 0 && dx < tw) to[dy * tw + dx] = ramp[level];
        }
    }
}

/* 0048c0a8: glyphs >= 0x21 advance by their own width, other codes by the
 * space's, '\n' moves down a line and back to x = 0, NUL ends. */
static void draw_with(const Ffn *f, const char *text, int x, int y, const uint8_t ramp[16])
{
    if (f->data == NULL) return;
    FfnGlyph space = { 0 };
    ffn_glyph(f, ' ', &space);
    int pen_x = x, pen_y = y;
    for (const unsigned char *c = (const unsigned char *)text; *c; c++) {
        FfnGlyph g;
        if (*c == '\n') {
            pen_x = 0;
            pen_y += f->line_height;
        } else if (*c < 0x21) {
            pen_x += space.advance;
        } else if (ffn_glyph(f, *c, &g)) {
            draw_glyph(f, &g, pen_x, pen_y, ramp);
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
    draw_with(&font, text, x, y, drawn_ramp);
}

void text_draw_shadowed(const char *text, int x, int y)
{
    draw_with(&font, text, x + 2, y + 2, shadow);
    draw_with(&font, text, x, y, current);
}

/* port: the small face's words, in the style and ramp text_select and text_ramp give text_draw */
int text_small_width(const char *text)
{
    return small_data != NULL ? ffn_text_width(&small, text) : 0;
}

int text_small_line_height(void)
{
    return small_data != NULL ? small.line_height : 0;
}

void text_small_draw(const char *text, int x, int y)
{
    draw_with(&small, text, x, y, drawn_ramp);
}

/* port: the level-th step from the pixel `under` towards colour `fg`, in the styles' palette */
static uint8_t blended(int fg, int under, int level)
{
    if (!blend_known[fg][under]) {
        palette_text_ramp(styles_pal, palette_usable(), fg, under, blend_ramps[fg][under]);
        blend_known[fg][under] = 1;
    }
    return blend_ramps[fg][under][level];
}

/* port: the glyphs' coverage num/den times the font's size into `mask` (mw x mh), the pen starting at (x, y) of it */
static void mask_glyph(const FfnGlyph *g, int pen_x, int pen_y, int num, int den, uint8_t *mask, int mw, int mh)
{
    const int w = ffn_scaled(g->width, num, den), h = ffn_scaled(g->height, num, den);
    const int left = pen_x + g->x_offset * num / den, top = pen_y + g->y_offset * num / den;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            if (top + y < 0 || top + y >= mh || left + x < 0 || left + x >= mw) continue;
            const int level = ffn_glyph_coverage_scaled(&font, g, x, y, num, den, TEXT_SOFT_SOLID_FROM);
            uint8_t *const at = &mask[(top + y) * mw + left + x];
            if (level > *at) *at = (uint8_t)level;
        }
}

static void mask_text(const char *text, int x, int y, int num, int den, uint8_t *mask, int mw, int mh)
{
    FfnGlyph space = { 0 };
    ffn_glyph(&font, ' ', &space);
    int pen = x * den;   /* in 1/den pixels, so the advances do not drift */
    for (const unsigned char *c = (const unsigned char *)text; *c; c++) {
        FfnGlyph g;
        if (*c < 0x21) {
            pen += space.advance * num;
        } else if (ffn_glyph(&font, *c, &g)) {
            mask_glyph(&g, pen / den, y, num, den, mask, mw, mh);
            pen += g.advance * num;
        }
    }
}

int text_width_scaled(const char *text, int num, int den)
{
    return text_width(text) * num / den;
}

/* port: the rows the text's glyphs reach, the line's height at least: '%' stands below the line (y 5..20 of 18) */
static int text_rows(const char *text)
{
    int rows = font.line_height;
    for (const unsigned char *c = (const unsigned char *)text; *c; c++) {
        FfnGlyph g;
        if (*c >= 0x21 && ffn_glyph(&font, *c, &g) && g.y_offset + g.height > rows) rows = g.y_offset + g.height;
    }
    return rows;
}

static int nearest(int r, int g, int b)
{
    return palette_nearest(styles_pal, palette_usable(), r, g, b);
}

/* the colour `index` times n/16 */
static int darkened(int index, int n)
{
    const uint8_t *const c = styles_pal + 3 * index;
    return nearest(c[0] * n / 16, c[1] * n / 16, c[2] * n / 16);
}

/* port: the coverage mask mw x mh (text_soft.h) drawn with its (0, 0) at (left, top) in the current style, over what
 * is under it; `shadow_of` says how dark the shadow is at a mask pixel */
static void draw_soft(const uint8_t *mask, int mw, int mh, int left, int top,
                      int (*shadow_of)(const uint8_t *mask, int w, int h, int x, int y))
{
    const int cream = current_style == TEXT_CREAM, yellow = current_style == TEXT_YELLOW;
    const int fill = cream ? nearest(TEXT_SOFT_CREAM_R, TEXT_SOFT_CREAM_G, TEXT_SOFT_CREAM_B) : styles_fg[current_style];
    const int grain = cream    ? nearest(TEXT_SOFT_GRAIN_R, TEXT_SOFT_GRAIN_G, TEXT_SOFT_GRAIN_B)
                      : yellow ? nearest(TEXT_SOFT_YELLOW_GRAIN_R, TEXT_SOFT_YELLOW_GRAIN_G, TEXT_SOFT_YELLOW_GRAIN_B)
                               : darkened(fill, 15);
    const int edge = darkened(fill, yellow ? TEXT_SOFT_YELLOW_EDGE_SIXTEENTHS : TEXT_SOFT_EDGE_SIXTEENTHS);
    const int lit_rim = current_style == TEXT_RED;   /* only the red titles have the light rim */
    const int rim = nearest(TEXT_SOFT_RIM_R, TEXT_SOFT_RIM_G, TEXT_SOFT_RIM_B), black = shadow[15];
    int tw, th;
    uint8_t *const to = video_target(&tw, &th);
    for (int my = 0; my < mh; my++) {
        const int ty = top + my;
        if (ty < 0 || ty >= th) continue;
        for (int mx = 0; mx < mw; mx++) {
            const int tx = left + mx;
            if (tx < 0 || tx >= tw) continue;
            uint8_t *const at = &to[ty * tw + tx];
            const TextSoftPart back = text_soft_back(mask, mw, mh, mx, my);
            if (back == TEXT_SOFT_RIM && lit_rim) *at = (uint8_t)rim;
            const int dark = shadow_of(mask, mw, mh, mx, my);
            if (back != TEXT_SOFT_RIM && dark > 0) *at = blended(black, *at, dark);
            const int coverage = mask[my * mw + mx];
            if (coverage == 0) continue;
            const int colour = text_soft_edge(mask, mw, mh, mx, my) ? edge : text_soft_grain(tx, ty) ? grain : fill;
            *at = coverage >= 15 ? (uint8_t)colour : blended(colour, *at, coverage);   /* the outline anti-aliased */
        }
    }
}

/* port: the menu art's item letters (text_soft.h) num/den times the font's size, over what is under them */
void text_draw_smooth(const char *text, int x, int y, int num, int den)
{
    enum { PAD = 1 + TEXT_SOFT_SHADOW_DEPTH };
    if (font_data == NULL) return;
    const int mw = text_width_scaled(text, num, den) + 2 * PAD, mh = text_rows(text) * num / den + 2 * PAD;
    uint8_t *const mask = calloc((size_t)mw * mh, 1);
    if (mask == NULL) return;
    mask_text(text, PAD, PAD, num, den, mask, mw, mh);
    draw_soft(mask, mw, mh, x - PAD, y - PAD, text_soft_shadow);
    free(mask);
}

void text_draw_mask(const uint8_t *mask, int w, int h, int x, int y)
{
    draw_soft(mask, w, h, x, y, text_soft_title_shadow);
}

void text_draw_item_mask(const uint8_t *mask, int w, int h, int x, int y)
{
    draw_soft(mask, w, h, x, y, text_soft_shadow);
}

void text_draw_header(const char *text, int x, int y, int num, int den, int underline)
{
    enum { PAD = 2, CAPACITY = 64, UNDERLINE_ROWS = 2 };
    if (font_data == NULL) return;
    char caps[CAPACITY];
    size_t n = 0;
    for (; text[n] != '\0' && n + 1 < CAPACITY; n++) caps[n] = (char)toupper((unsigned char)text[n]);
    caps[n] = '\0';
    const int mw = text_width_scaled(caps, num, den) + 2 * PAD, mh = font.line_height * num / den + 2 * PAD;
    uint8_t *const mask = calloc((size_t)mw * mh, 1);
    if (mask == NULL) return;
    mask_text(caps, PAD, PAD, num, den, mask, mw, mh);
    const int tan = nearest(TEXT_SOFT_HEADER_R, TEXT_SOFT_HEADER_G, TEXT_SOFT_HEADER_B), black = shadow[15];
    int tw, th;
    uint8_t *const to = video_target(&tw, &th);
    const int left = x - PAD, top = y - PAD;
    for (int my = 0; my < mh; my++)
        for (int mx = 0; mx < mw; mx++) {
            const int tx = left + mx, ty = top + my;
            if (tx < 0 || tx >= tw || ty < 0 || ty >= th) continue;
            uint8_t *const at = &to[ty * tw + tx];
            const int dark = text_soft_header_shadow(mask, mw, mh, mx, my);
            if (dark > 0) *at = dark >= 15 ? (uint8_t)black : blended(black, *at, dark);
            const int coverage = mask[my * mw + mx];
            if (coverage > 0) *at = coverage >= 15 ? (uint8_t)tan : blended(tan, *at, coverage);
        }
    const int last = text_soft_last_row(mask, mw, mh);
    const int length = underline > 0 ? underline : mw - 2 * PAD;
    free(mask);
    if (last < 0) return;
    const int line = top + last + TEXT_SOFT_HEADER_UNDERLINE_GAP;
    for (int row = 0; row < UNDERLINE_ROWS; row++) {
        if (line + row < 0 || line + row >= th) continue;
        for (int tx = x; tx < x + length; tx++)
            if (tx >= 0 && tx < tw) to[(line + row) * tw + tx] = (uint8_t)(row == 0 ? tan : black);
    }
}

void text_draw_centered(const char *text, int cx, int y)
{
    text_draw_shadowed(text, cx - (short)text_width(text) / 2, y);
}
