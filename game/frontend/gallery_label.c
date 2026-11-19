/* See gallery_label.h. */
#include "game/frontend/gallery_label.h"

#include "game/sys/text.h"

#include <stdlib.h>

enum { COVERED = 6 };   /* the coverage (0..15) from which a mask pixel is solid */

void gallery_label_draw(uint8_t *to, int tw, int th, const char *text, int x, int y, int h, int index, int shadow)
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
        const int px0 = x + GALLERY_LABEL_MARGIN - left, py0 = y + h - 1 - GALLERY_LABEL_MARGIN - bottom;
        for (int pass = shadow < 0 ? 1 : 0; pass < 2; pass++)
            for (int j = 0; j < rows; j++)
                for (int i = 0; i < w; i++) {
                    const int px = px0 + i + (pass == 0), py = py0 + j + (pass == 0);
                    if (mask[j * w + i] >= COVERED && px >= 0 && px < tw && py >= 0 && py < th) to[py * tw + px] = (uint8_t)(pass == 0 ? shadow : index);
                }
    }
    free(mask);
}

void gallery_label_tile(uint8_t *to, int tw, int th, const char *text, int x, int y, int h, int black, int yellow, int lit)
{
    gallery_label_draw(to, tw, th, text, x, y, h, lit ? yellow : black, -1);
}
