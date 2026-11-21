/* port (dep59 step 3, the owner's): a picture put in the palette of the screen it is drawn on, as that screen's art is. In the core, so the game's menus (an added car's
 * card.png on Control Central) and the Studio («As on the card») run the same code. The screen's palette is never changed. */
#ifndef TNFS_RASTER_PICTURE_PALETTE_H
#define TNFS_RASTER_PICTURE_PALETTE_H

#include <stdint.h>

/* a `w` x `h` picture dithered: Floyd-Steinberg error diffusion in the palette's 8 bits a channel
 * (each pixel the nearest usable entry to its colour plus the error carried to it, the error kept within 0..255), so a colour the
 * screen's palette lacks comes out as a mix of the ones it has; entry 0 everywhere when none is marked */
void picture_palette_dither(const uint8_t *rgba, int w, int h, const uint8_t palette[768], const uint8_t usable[256], uint8_t *out);

#endif
