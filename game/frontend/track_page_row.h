/* port (dep59 step 5, T7d): the bottom row of the CD's track page (DONE, "< Road/track >", Best times), cut from a stock track's bank
 * (FRONTEND/TRACKSEL/CITYSEL.QFS: its background with the three unlit and the lit shapes donl, prel, nexl, besl) so the added track's page
 * has them in the same pixels and look. As gallery_layout.h's GalleryRow does for the car pages, the letters' and arrows' own pixels
 * ("faces": light, cream or yellow) in five looks, plain and with DONE, Previous, Next or Best lit; the page's own picture shows
 * elsewhere, so the art's other colours are never drawn. The row spans the screen's width here (the car pages' is 420 wide). */
#ifndef TNFS_TRACK_PAGE_ROW_H
#define TNFS_TRACK_PAGE_ROW_H

#include <stddef.h>
#include <stdint.h>

enum {
    TR_Y = 436, TR_H = 30, TR_W = 640, TR_SHADOW = 2, TR_COLOURS = 64,
    TR_PLAIN = 0, TR_DONE = 1, TR_PREVIOUS = 2, TR_NEXT = 3, TR_BEST = 4, TR_LOOKS = 5
};

typedef struct {
    uint8_t face[TR_LOOKS][TR_H * TR_W];   /* 0 none; k: the colour rgb[k - 1] */
    uint8_t rgb[TR_COLOURS][3];
    int count[TR_COLOURS];                 /* the pixels of each colour over the looks */
    int colours;
} TrackRow;

void track_row_clear(TrackRow *row);
/* a picture of the bank (w x h, its top left at (x0, y0), entries of the palette `pal` 8 bits a channel; the texel 0xff is the masked
 * blit's transparent one) into the look: inside its rectangle on the row, its faces and nothing else; past TR_COLOURS colours, the
 * nearest of those kept */
void track_row_put(TrackRow *row, int look, const uint8_t *pixels, int w, int h, int x0, int y0, const uint8_t pal[768]);
/* the five looks from a bank's file (QFS or SHPI): its background, and over it in turn the lit shapes donl, prel, nexl, besl; 1 when it
 * had them all and the palette */
int track_row_cut(TrackRow *row, const uint8_t *file, size_t size);
/* the look for the page's item (TP_*): plain for a row of the left */
int track_row_look(int item);
/* the look into `to` (tw x th): a shadow in entry `shadow` TR_SHADOW px down and right of the faces, then the faces, colour k in entry map[k - 1] */
void track_row_draw(const TrackRow *row, int look, uint8_t *to, int tw, int th, const uint8_t map[TR_COLOURS], int shadow);

#endif
