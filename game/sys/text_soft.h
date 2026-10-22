/* port: the soft look of the port's large text (text_draw_smooth): a blurred shadow, and the letters' anti-aliasing
 * levels lifted so strokes reach the full colour and only their edges blend. Levels are the font's, 0..15. */
#ifndef TNFS_TEXT_SOFT_H
#define TNFS_TEXT_SOFT_H

#include <stdint.h>

enum {
    TEXT_SOFT_SHADOW_PERCENT = 80,   /* the shadow's darkest, of the text's level */
    TEXT_SOFT_SHADOW_PUSH = 1,       /* past the game's 2 px offset: the blur spreads a pixel back under the letters */
    TEXT_SOFT_LIFT_NUM = 3, TEXT_SOFT_LIFT_DEN = 2
};

/* `out` = the 3x3 box blur of `in` (w x h, outside counts 0), scaled by TEXT_SOFT_SHADOW_PERCENT, rounded */
void text_soft_shadow(const uint8_t *in, uint8_t *out, int w, int h);
/* a letter's level lifted towards 15 (0 stays 0) */
int text_soft_lift(int level);

#endif
