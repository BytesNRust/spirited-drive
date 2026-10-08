/* Front-end text: fonts/graver18.ffn drawn with a 16-step anti-aliasing ramp
 * into the 8-bit back buffer. Each style is a colour matched to the current
 * palette; level 0 of a glyph is transparent, levels 1-15 use the ramp. */
#ifndef TNFS_TEXT_H
#define TNFS_TEXT_H

typedef enum {
    TEXT_CREAM = 0,   /* #E8D3B7, the default (00522118) */
    TEXT_YELLOW = 1,  /* #FFCE06 (00523158) */
    TEXT_RED = 2,     /* #FF0606 (0051ed58) */
    TEXT_WHITE = 3    /* #FFFFFF (005210d8) */
} TextStyle;

int text_init(void);                                    /* 004322f8: load + 00487e10 */
void text_free(void);

void text_build_styles(void);                           /* 0046376c, from the current palette */
void text_select(TextStyle style);                      /* 004638c0 */
int text_width(const char *text);                       /* 004770f4 */
int text_line_height(void);
void text_draw(const char *text, int x, int y);         /* 0048c264 */
void text_draw_shadowed(const char *text, int x, int y); /* 00463cd8: black shadow at +2,+2 */
void text_draw_centered(const char *text, int cx, int y); /* 00463c60: same, centred on cx */

#endif
