/* port (dep59 step 5, T7d): an added track's page laid out like the CD's track page: the configuration screens' sepia background (turned
 * to the mod's showcase_tint when it gives one) with the Road & Track band under the title covered by its own texture mirrored, the big
 * box at the right (as the CD's page has the map's) holding the track's card, and the rows, title and bottom row the glue draws over it.
 * Made once in RGB. No drawing and no files here. */
#ifndef TNFS_TRACK_PAGE_LAYOUT_H
#define TNFS_TRACK_PAGE_LAYOUT_H

#include <stdint.h>

enum {
    TPL_W = 640, TPL_H = 480,
    /* the band's patch: the band's texture from TPL_BAND_FROM_X, mirrored, over 0 .. TPL_BAND_W - 1 (the car page's, gallery_layout.h) */
    TPL_BAND_W = 285, TPL_BAND_H = 56, TPL_BAND_FROM_X = 300,
    /* the big box (the CD's page: x 308..610), its card on top, then the name, the author, the length and the description's window */
    /* the owner's (2026-11-18): the CD's circuit page's box (Rusty: x 310..606 from y 85, under the title's line, so the title has its whole
     * strip) taken down to the Weather row (y 432); measured on his captures of the Rusty and City pages */
    TPL_BOX_X0 = 310, TPL_BOX_Y0 = 85, TPL_BOX_X1 = 606, TPL_BOX_Y1 = 432, TPL_SHADOW = 6,
    TPL_CARD_W = 208, TPL_CARD_H = 127, TPL_CARD_X = TPL_BOX_X0 + (TPL_BOX_X1 - TPL_BOX_X0 + 1 - TPL_CARD_W) / 2, TPL_CARD_Y = TPL_BOX_Y0 + 11,
    TPL_TEXT_X = TPL_BOX_X0 + 16, TPL_TEXT_X1 = TPL_BOX_X1 - 16,
    TPL_NAME_Y = TPL_CARD_Y + TPL_CARD_H + 8, TPL_AUTHOR_Y = TPL_NAME_Y + 24, TPL_LENGTH_Y = TPL_AUTHOR_Y + 18,
    TPL_RULE_Y = TPL_LENGTH_Y + 22, TPL_DESC_Y0 = TPL_RULE_Y + 6, TPL_DESC_Y1 = TPL_BOX_Y1 - 8
};

/* the page in RGB (TPL_W x TPL_H x 3) into `out` from the background `bgnd` (the same size) and the card `card` (TPL_CARD_W x TPL_CARD_H x 3);
 * `tint` NULL leaves the background's sepia as it is, else turns it to that colour (showcase_tint.h) */
void track_page_compose(const uint8_t *bgnd, const uint8_t *card, const uint8_t tint[3], uint8_t *out);

#endif
