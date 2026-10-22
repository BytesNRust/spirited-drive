/* port: the look of the port's large text (text_draw_smooth), as the menu art draws its items: solid letters with
 * a grain of a second tone and only their outline anti-aliased, a darker edge facing down-right, a light rim outside
 * the edge facing up-left and a hard near-black shadow. The mask holds each pixel's coverage by the letters, 0..15. */
#ifndef TNFS_TEXT_SOFT_H
#define TNFS_TEXT_SOFT_H

#include <stdint.h>

enum {
    TEXT_SOFT_SOLID_FROM = 8,     /* the scaled glyph's level its outline runs at, and the coverage of a letter pixel */
    TEXT_SOFT_SHADOW_DEPTH = 2,   /* the art's shadow: the letters pushed 1 and 2 px down-right */
    TEXT_SOFT_SHADOW_LEVEL = 12,  /* of 15 from what is under it towards black */
    /* the drawn items' cream and its grain (Network, Exit in Game Mode) */
    TEXT_SOFT_CREAM_R = 238, TEXT_SOFT_CREAM_G = 214, TEXT_SOFT_CREAM_B = 182,
    TEXT_SOFT_GRAIN_R = 222, TEXT_SOFT_GRAIN_G = 206, TEXT_SOFT_GRAIN_B = 190,
    /* the light rim of the highlighted and title letters */
    TEXT_SOFT_RIM_R = 230, TEXT_SOFT_RIM_G = 222, TEXT_SOFT_RIM_B = 214,
    TEXT_SOFT_EDGE_SIXTEENTHS = 14   /* the edge facing down-right: the letter's colour times 14/16 */
};

typedef enum {
    TEXT_SOFT_NONE,
    TEXT_SOFT_RIM,      /* outside, just up-left of a letter */
    TEXT_SOFT_SHADOW    /* within TEXT_SOFT_SHADOW_DEPTH down-right of a letter (text_soft_shadow says how dark) */
} TextSoftPart;

/* what lies under the letters at (x, y) of the coverage mask (w x h, outside counts 0) */
TextSoftPart text_soft_back(const uint8_t *mask, int w, int h, int x, int y);
/* 1 where the letters' colour at (x, y) is the darker edge's: the pixel down-right is not letter */
int text_soft_edge(const uint8_t *mask, int w, int h, int x, int y);
/* how dark the shadow is at (x, y), 0..TEXT_SOFT_SHADOW_LEVEL: the letters' coverage pushed down-right, so its
 * outline is anti-aliased like theirs */
int text_soft_shadow(const uint8_t *mask, int w, int h, int x, int y);
/* 1 where the fill takes its grain tone instead, about one pixel in four, fixed by the screen position so a
 * redrawn label stays the same */
int text_soft_grain(int x, int y);

#endif
