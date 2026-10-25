/* port: the hand-made masks of the cars' body textures. One PNG a texture, <root>/<CAR>/<tex>_mask.png (data/car_masks in the
 * source tree): a texel that is not black is kept as it is (never recoloured), a black one is painted. The mask has the size of the
 * high detail bank's texture; the low detail bank's, of another size, takes it scaled by nearest neighbour. A texture with a mask is
 * repainted by it alone (car_repaint.h's RepaintTexture.mask); one without keeps the family rules. The masks hold no colour. */
#ifndef TNFS_CAR_MASK_H
#define TNFS_CAR_MASK_H

#include <stddef.h>
#include <stdint.h>

/* Decodes a mask PNG into `out` (width x height bytes: 1 keep, 0 paint), scaled from the image's own size by nearest neighbour. An
 * indexed image keeps where its palette colour is not black; any other image where one of its colour channels is above 0. Returns 0
 * for data that is not a readable PNG or a zero size. */
int car_mask_parse(const uint8_t *png, size_t len, int width, int height, uint8_t *out);

/* Writes `keep` (width x height: non-zero keeps) as the mask's PNG: indexed, black (paint) and white (keep). `*out` is freed with
 * png_free. Returns 0 when it cannot. */
int car_mask_write(const uint8_t *keep, int width, int height, uint8_t **out, size_t *len);

/* The mask of the car `stem` ("TSupra", as the PC names it)'s texture `name` (4 letters) for a texture of width x height texels,
 * from the first of $TNFS_CAR_MASKS, data/car_masks and the source tree's data/car_masks that has the file. malloc'd; NULL when there
 * is none or it does not read: the texture then follows the rules. */
uint8_t *car_mask_load(const char *stem, const char name[4], int width, int height);

#endif
