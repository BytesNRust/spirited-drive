/* port: the menus' small labels (a tile's name, DONE, the column heads) drawn as the CD draws its own: the main face's coverage mask
 * (text_mask_scaled: by area, 0..15) blended with whatever is under it through the palette's ramps (palette_text_ramp) instead of cut
 * to one bit, so the letters have the CD's soft edge on a photo or a flat page alike. Into any 8-bit picture, not only the screen. */
#ifndef TNFS_TEXT_LABEL_H
#define TNFS_TEXT_LABEL_H

#include <stdint.h>

typedef struct {
    int fill;        /* the letters' palette entry */
    int relief;      /* 1: a grain (15/16 of the fill) in about one pixel in four and the edge facing down-right (14/16), as the items' look */
    int shadow;      /* the shadow's palette entry, -1 none */
    int shadow_px;   /* how many pixels down and right the shadow lies */
} TextLabelLook;

/* the coverage mask `mask` (w x h, 0..15) with its (0, 0) at (x, y) of the picture `to` (tw x th, pixels indices of `palette`, 768 bytes) */
void text_label_blit_mask(uint8_t *to, int tw, int th, const uint8_t palette[768], const uint8_t *mask, int w, int h, int x, int y,
                          const TextLabelLook *look);

/* `text` at num/den of the main face with its pen at (x, y): its mask made and blitted. 0 when the font is not loaded */
int text_label_draw(uint8_t *to, int tw, int th, const uint8_t palette[768], const char *text, int x, int y, int num, int den,
                    const TextLabelLook *look);

#endif
