/* port (dep59 step 3): an added car's card and rival card for Control Central, from its piece's pictures (notes/dep59-step3-card-
 * gallery-plan.md, Task 8): card.png (309x166) where the exe's card is, and rival.png (284x117) where its rival card is, or card.png's
 * middle when the piece has no rival.png. Each picture is put in the screen's palette (picture_palette.h) and made a shape bank whose
 * "0000" is it at its place, as the CD's control/<car>1.qfs and <car>2.qfs are. The game does not scale: a picture of another size is
 * refused. */
#ifndef TNFS_ADDED_CARD_H
#define TNFS_ADDED_CARD_H

#include "formats/png.h"

#include <stddef.h>
#include <stdint.h>

enum {
    ADDED_CARD_W = 309, ADDED_CARD_H = 166, ADDED_CARD_X = 21, ADDED_CARD_Y = 114,      /* the CD's car cards' "0000" */
    ADDED_RIVAL_W = 284, ADDED_RIVAL_H = 117, ADDED_RIVAL_X = 341, ADDED_RIVAL_Y = 99   /* its rival cards' */
};

/* `img` (RGBA, alpha ignored, or indexed through its own palette) as the card (`rival` 0) or the rival card (1; `from_card` 1: `img` is
 * card.png and its middle is taken), each pixel the nearest of `palette`'s entries `usable` marks: a malloc'd shape bank and its size;
 * NULL with the reason in `why` when the picture is not its size or memory ran out */
uint8_t *added_card_bank(const PngImage *img, int rival, int from_card, const uint8_t palette[768], const uint8_t usable[256], size_t *size,
                         char *why, size_t why_cap);

#endif
