/* port (dep59 showcase, PR B): a photo made to fit a tile, and the placeholder a tile shows when the piece brings no photo. No files here. */
#ifndef TNFS_GALLERY_PHOTO_H
#define TNFS_GALLERY_PHOTO_H

#include <stdint.h>

/* `src` (sw x sh x 3, RGB) into `dst` (dw x dh x 3) cropped to fill: scaled by the one factor that covers dst, the middle kept, averaged where
 * it is shrunk (a box of source pixels per pixel) and read nearest where it is enlarged. 0 when a size is not positive */
int gallery_cover(const uint8_t *src, int sw, int sh, uint8_t *dst, int dw, int dh);

/* the placeholder (dw x dh x 3): the page's own texture `ground` (gw x gh x 3, from its top left) darkened, with a lighter frame inside, as the
 * CD's tiles have when a photo is missing. 0 when `ground` is smaller than the tile */
int gallery_placeholder(const uint8_t *ground, int gw, int gh, uint8_t *dst, int dw, int dh);

#endif
