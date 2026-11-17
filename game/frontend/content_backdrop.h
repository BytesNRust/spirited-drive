/* port (dep59 step 4b): the CONTENT screen's colours matched in its background's own palette (the configuration screens' clean one):
 * the background darkened toward a colour, a flat colour's entry, and the set a whole picture (the preview) is matched to. Every entry
 * may be picked but the pointer's and the see-through one; palette_usable() is not read, as it holds the set of the last picture that
 * set it (Game Mode's art, the race's), not this palette's. */
#ifndef TNFS_CONTENT_BACKDROP_H
#define TNFS_CONTENT_BACKDROP_H

#include <stdint.h>

enum { CONTENT_BACKDROP_POINTER = 0xfe, CONTENT_BACKDROP_CLEAR = 0xff };

/* usable[i] 1 for every entry but the pointer's and the see-through one */
void content_backdrop_usable(uint8_t usable[256]);
/* table[i]: the entry of `pal` nearest pal[i] taken num/den of the way to 0xRRGGBB `rgb`; the pointer's and the see-through one kept */
void content_backdrop_darken(const uint8_t pal[768], uint32_t rgb, int num, int den, uint8_t table[256]);
/* the card's panel on a w x h picture: inside x0..x1, y0..y1 (inclusive, clipped to the picture) every pixel through `table` (a
 * darkening, content_backdrop_darken), its one-pixel border the entry `edge` */
void content_backdrop_panel(uint8_t *pixels, int w, int h, const uint8_t table[256], int x0, int y0, int x1, int y1, uint8_t edge);
/* the entry of `pal` nearest 0xRRGGBB `rgb` */
int content_backdrop_nearest(const uint8_t pal[768], uint32_t rgb);

#endif
