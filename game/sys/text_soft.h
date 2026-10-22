/* port: the soft look of the port's large text (text_draw_smooth): a blurred shadow, and the letters' anti-aliasing
 * levels lifted so strokes reach the full colour and only their edges blend. Levels are the font's, 0..15. */
#ifndef TNFS_TEXT_SOFT_H
#define TNFS_TEXT_SOFT_H

#include <stdint.h>

enum {
    TEXT_SOFT_SHADOW_PERCENT = 90,   /* the shadow's darkest, of the text's level */
    /* the drawn items' light cream (their bevel's highlights, in Game Mode's and Options' palettes), lighter and as
     * warm as the font's #E8D3B7 */
    TEXT_SOFT_CREAM_R = 246, TEXT_SOFT_CREAM_G = 222, TEXT_SOFT_CREAM_B = 190,
    TEXT_SOFT_SHADOW_PUSH = 1,       /* past the game's 2 px offset: the blur spreads a pixel back under the letters */
    TEXT_SOFT_LIFT_NUM = 3, TEXT_SOFT_LIFT_DEN = 2
};

/* `out` = `in` (w x h, outside counts 0) through the 1-4-1 kernel each way, scaled by TEXT_SOFT_SHADOW_PERCENT,
 * rounded: a light blur that keeps the drawn items' sharp shadow, only its edge softened */
void text_soft_shadow(const uint8_t *in, uint8_t *out, int w, int h);
/* a letter's level lifted towards 15 (0 stays 0) */
int text_soft_lift(int level);


#endif
