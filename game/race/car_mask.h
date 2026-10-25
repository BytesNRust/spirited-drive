/* port: the hand-made masks of the cars' body textures. One PNG a texture, <root>/<CAR>/<tex>_mask.png (data/car_masks in the
 * source tree), three values: 0 a texel that is painted; 128 one kept as it is that takes the body shine (glass, lamps); 255 one kept as it
 * is without it (plastics, trim, plate, badges, vents). The repaint only asks paint or not (any non-zero is kept); the class is for the
 * shine pass. The mask has the size of the
 * high detail bank's texture; the low detail bank's, of another size, takes it scaled by nearest neighbour. A texture with a mask is
 * repainted by it alone (car_repaint.h's RepaintTexture.mask); one without keeps the family rules. The masks hold no colour. */
#ifndef TNFS_CAR_MASK_H
#define TNFS_CAR_MASK_H

#include <stddef.h>
#include <stdint.h>

enum { CAR_MASK_PAINT = 0, CAR_MASK_SHINE = 128, CAR_MASK_MATTE = 255 };

/* Decodes a mask PNG into `out` (width x height bytes of CAR_MASK_*), scaled from the image's own size by nearest neighbour. A texel is
 * classed by its colour (an indexed image's palette colour): black paints; a blue one (blue above red and green) or a dim one (no channel
 * of 192) shines; any other is matte. Returns 0 for data that is not a readable PNG or a zero size. */
int car_mask_parse(const uint8_t *png, size_t len, int width, int height, uint8_t *out);

/* Writes `mask` (width x height of CAR_MASK_*; any other non-zero value is matte) as the mask's PNG: indexed, black, grey 128 and white.
 * `*out` is freed with png_free. Returns 0 when it cannot. */
int car_mask_write(const uint8_t *mask, int width, int height, uint8_t **out, size_t *len);

/* The mask of the car `stem` ("TSupra", as the PC names it)'s texture `name` (4 letters) for a texture of width x height texels,
 * from the first of $TNFS_CAR_MASKS, data/car_masks and the source tree's data/car_masks that has the file. malloc'd; NULL when there
 * is none or it does not read: the texture then follows the rules. */
uint8_t *car_mask_load(const char *stem, const char name[4], int width, int height);

#endif
