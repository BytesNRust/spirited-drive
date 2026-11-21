/* port (dep59 step 3): a picture of a mod's (an added car's card.png) put in the palette of the screen it is drawn on, as that screen's
 * art is: each pixel the nearest entry by squared distance among those the screen lets it use (its pointer colours and 0xff are the
 * screen's own). The screen's palette is never changed. */
#ifndef TNFS_PICTURE_PALETTE_H
#define TNFS_PICTURE_PALETTE_H

#include <stdint.h>

/* `n` RGBA pixels (alpha ignored) as indices of `palette` (768 bytes, 8 bits a channel) into `out`: each the nearest of the entries
 * `usable` marks; entry 0 when none is marked */
void picture_palette_nearest(const uint8_t *rgba, int n, const uint8_t palette[768], const uint8_t usable[256], uint8_t *out);

#include "raster/picture_palette.h"   /* picture_palette_dither: in the core */

#endif
