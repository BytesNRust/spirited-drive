/* The menus on a wide screen: the 640 x 480 picture centred over a darkened, enlarged copy of itself. Port glue. */
#ifndef TNFS_BACKDROP_H
#define TNFS_BACKDROP_H

#include <stdint.h>

/* For each colour, the palette's colour nearest to 45 % of its brightness (squared distance over the channels, the
 * lowest index on a tie). rgb: 256 entries of 3 bytes. */
void backdrop_shade_table(const uint8_t rgb[768], uint8_t out[256]);

/* dst (dst_w x 480): src (640 x 480) enlarged by nearest neighbour by dst_w / 640 to cover dst_w, cut equally at the
 * top and bottom, and darkened with `shade`; over it src untouched at x = (dst_w - 640) / 2. dst_w 640 is src. */
void backdrop_compose(const uint8_t *src, uint8_t *dst, int dst_w, const uint8_t shade[256]);

#endif
