/* port: the hand-made masks of the cars' body textures. One PNG a texture and detail level: <root>/<CAR>/<tex>_mask.png for the high
 * detail bank (data/car_masks in the source tree), <tex>_lo_mask.png for the low detail bank (its textures are of other sizes, so they
 * have their own files). Five values: 0 a texel that is painted; 128 one kept as it is that takes the glass shine (glass, lamps); 192 one kept as it is that shines as the
 * body does (a two-tone car's roof and pillars: black in the texture, but body); 224 one repainted with the car's black whatever paint the car has
 * (car_repaint_fixed_black: a two-tone car's black roof whose texture is light grey), and shining as paint; 255 one
 * kept as it is without it (plastics, trim, plate, badges, vents). The repaint only asks paint or not (any non-zero is kept); the class is
 * for the shine pass. The mask has the size of its texture, or any other size, scaled by nearest neighbour. A texture with a mask is
 * repainted by it alone (car_repaint.h's RepaintTexture.mask); one without keeps the family rules. The masks hold no colour. */
#ifndef TNFS_CAR_MASK_H
#define TNFS_CAR_MASK_H

#include <stddef.h>
#include <stdint.h>

enum { CAR_MASK_PAINT = 0, CAR_MASK_SHINE = 128, CAR_MASK_BODY = 192, CAR_MASK_BLACK = 224, CAR_MASK_BRAKE = 240, CAR_MASK_POSITION = 244, CAR_MASK_HEADLIGHT = 248, CAR_MASK_MATTE = 255 };
/* port: 240 a brake light, 244 a rear position light, 248 a headlight: texels kept as they are that glow when the light is low (the greys 240..243, 244..247 and
 * 248..251 read as those; 252 to 255 are matte). */

/* Decodes a mask PNG into `out` (width x height bytes of CAR_MASK_*), scaled from the image's own size by nearest neighbour. A texel is
 * classed by its colour (an indexed image's palette colour): black paints; a blue one (blue above red and green) or a dim one (no channel
 * of 192) shines as glass; a green one (green above red and blue) or a grey of 192 to 223 shines as the body; a magenta one (red and blue well above green) or a grey of 224 to 239 is the fixed black; a grey of 240 to 251 is a brake, position or head light (four values each); any other is matte. Returns 0 for data that is not a readable PNG or a zero size. */
int car_mask_parse(const uint8_t *png, size_t len, int width, int height, uint8_t *out);

/* Writes `mask` (width x height of CAR_MASK_*; any other non-zero value is matte) as the mask's PNG: indexed, black, grey 128, grey 192, grey 224, the three light greys and white.
 * `*out` is freed with png_free. Returns 0 when it cannot. */
int car_mask_write(const uint8_t *mask, int width, int height, uint8_t **out, size_t *len);

/* The mask of the car `stem` ("TSupra", as the PC names it)'s texture `name` (4 letters) at detail `level` (0 high, 1 low: the file is
 * <tex>_mask.png, or <tex>_lo_mask.png for the low one, which never falls back on the high one's file) for a texture of width x height
 * texels, from the first of $TNFS_CAR_MASKS, data/car_masks and the source tree's data/car_masks that has the file. malloc'd; NULL when
 * there is none or it does not read: the texture then follows the rules. */
uint8_t *car_mask_load(const char *stem, const char name[4], int level, int width, int height);

#endif
