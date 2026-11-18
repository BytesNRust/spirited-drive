/* port (dep59 showcase, owner 2026-11-18): a tile's label as the CD's pages have it. Measured on the CD's GENERAL (512TR's page, the tile at
 * 133,173, 163 x 109): the main face at full size, its ink 7 pixels from the tile's left edge and 7 from its bottom, black and with nothing
 * white about it. The label is placed by its ink, not by the pen, so every tile of the added car's page and the SETUP tile of the CD pages have
 * exactly those margins. */
#ifndef TNFS_GALLERY_LABEL_H
#define TNFS_GALLERY_LABEL_H

#include <stdint.h>

enum { GALLERY_LABEL_MARGIN = 7 };

/* `text` in the tile (x, y, h tall) of the 8-bit picture `to` (tw x th): solid `index`, and with `shadow` >= 0 a one-pixel shadow of that entry down
 * and right (the lit tile's) */
void gallery_label_draw(uint8_t *to, int tw, int th, const char *text, int x, int y, int h, int index, int shadow);

#endif
