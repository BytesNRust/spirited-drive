/* Front-end text: fonts/graver18.ffn drawn with a 16-step anti-aliasing ramp
 * into the 8-bit back buffer. Each style is a colour matched to the current
 * palette; level 0 of a glyph is transparent, levels 1-15 use the ramp. */
#ifndef TNFS_TEXT_H
#define TNFS_TEXT_H

#include <stdint.h>

typedef enum {
    TEXT_CREAM = 0,   /* #E8D3B7, the default (00522118) */
    TEXT_YELLOW = 1,  /* #FFCE06 (00523158) */
    TEXT_RED = 2,     /* #FF0606 (0051ed58) */
    TEXT_WHITE = 3,   /* #FFFFFF (005210d8) */
    TEXT_DIM = 4      /* port: #8C8273, a cream held back for the rows a screen does not let the player change */
} TextStyle;

int text_init(void);                                    /* 004322f8: load + 00487e10 */
void text_free(void);

void text_build_styles(void);                           /* 0046376c, from the current palette */
void text_build_styles_from(const uint8_t pal[768]);    /* port: the same from a palette not yet on screen */
void text_select(TextStyle style);                      /* 004638c0 */
int text_width(const char *text);                       /* 004770f4 */
int text_line_height(void);
void text_ramp(int use_shadow);                         /* 0048ec60: text_draw's ramp, the shadow's or the style's */
void text_draw(const char *text, int x, int y);         /* 0048c264 */
void text_draw_shadowed(const char *text, int x, int y); /* 00463cd8: black shadow at +2,+2 */
void text_draw_centered(const char *text, int cx, int y); /* 00463c60: same, centred on cx */
/* port: text_width and text_draw_shadowed num/den times the font's size (one line), the edges mixed with what is
 * under them in the palette text_build_styles* last used */
int text_width_scaled(const char *text, int num, int den);
void text_draw_smooth(const char *text, int x, int y, int num, int den);
/* port: a title's own letters, a w x h coverage mask (0..15, as title_art.h cuts them), drawn as text_draw_smooth
 * draws its letters in the current style but with the original titles' shadow (text_soft_title_shadow), the mask's
 * (0, 0) at (x, y) */
void text_draw_mask(const uint8_t *mask, int w, int h, int x, int y);
/* port: an item's own letters (item_art.h), a w x h coverage mask drawn as text_draw_smooth draws the items: in the
 * current style, with the item shadow (text_soft_shadow), the mask's (0, 0) at (x, y) */
void text_draw_item_mask(const uint8_t *mask, int w, int h, int x, int y);
/* port: a section header as the credits draw theirs (text_soft.h), in capitals num/den times the font's size, with
 * its underline `underline` pixels long from x (the text's width when 0) */
void text_draw_header(const char *text, int x, int y, int num, int den, int underline);
/* port: the CD's small face, FRONTEND/ART/MAIN18.FFN (13 px capitals, 20 px lines; graver18 is 15 and 24), loaded on its
 * own by text_small_init and drawn with the style and ramp text_select and text_ramp set, as text_draw is. The
 * glyphs are the CD's own; nothing is scaled. */
int text_small_init(void);
int text_small_width(const char *text);
int text_small_line_height(void);
void text_small_draw(const char *text, int x, int y);

#endif
