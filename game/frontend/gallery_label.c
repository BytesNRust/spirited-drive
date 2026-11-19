/* See gallery_label.h. */
#include "game/frontend/gallery_label.h"

#include "game/sys/text.h"
#include "game/sys/text_label.h"

#include <stdlib.h>

enum { COVERED = 6 };   /* the coverage (0..15) from which a mask pixel is solid */

void gallery_label_draw(uint8_t *to, int tw, int th, const uint8_t palette[768], const char *text, int x, int y, int h, int index, int lit_colour)
{
    const int w = text_width(text) + 2, rows = text_mask_rows(text, 1, 1);
    uint8_t *mask = rows > 0 ? malloc((size_t)w * rows) : NULL;
    if (mask == NULL || !text_mask_scaled(text, 1, 1, mask, w, rows)) {
        free(mask);
        return;
    }
    int left = w, bottom = -1;   /* the ink's first column and last row in the mask */
    for (int j = 0; j < rows; j++)
        for (int i = 0; i < w; i++)
            if (mask[j * w + i] >= COVERED) {
                if (i < left) left = i;
                if (j > bottom) bottom = j;
            }
    if (bottom >= 0) {
        /* the CD's: unlit, black with soft edges and nothing else; lit, yellow with the items' grain and darker edge */
        const TextLabelLook look = { lit_colour >= 0 ? lit_colour : index, lit_colour >= 0, -1, 0 };
        text_label_blit_mask(to, tw, th, palette, mask, w, rows, x + GALLERY_LABEL_MARGIN - left, y + h - 1 - GALLERY_LABEL_MARGIN - bottom, &look);
    }
    free(mask);
}

void gallery_label_tile(uint8_t *to, int tw, int th, const uint8_t palette[768], const char *text, int x, int y, int h, int black, int yellow, int lit)
{
    gallery_label_draw(to, tw, th, palette, text, x, y, h, black, lit ? yellow : -1);
}
