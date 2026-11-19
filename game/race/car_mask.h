/* port: the loads of the hand-made masks (the files of a folder, by the car's stem); the classes and the PNG codec are core's (formats/car_mask_codec.h:
 * what a mask holds, car_mask_parse and car_mask_write). */
#ifndef TNFS_CAR_MASK_H
#define TNFS_CAR_MASK_H

#include "formats/car_mask_codec.h"

#include <stddef.h>
#include <stdint.h>

/* The mask of the car `stem` ("TSupra", as the PC names it)'s texture `name` (4 letters) at detail `level` (0 high, 1 low: the file is
 * <tex>_mask.png, or <tex>_lo_mask.png for the low one, which never falls back on the high one's file) for a texture of width x height
 * texels, from the first of $TNFS_CAR_MASKS, data/car_masks and the source tree's data/car_masks that has the file. malloc'd; NULL when
 * there is none or it does not read: the texture then follows the rules. */
uint8_t *car_mask_load(const char *stem, const char name[4], int level, int width, int height);

/* port (dep59 step 3): the same from <dir>/<tex>[_lo]_mask.png alone, an added car's skin's folder or its car_masks/<STEM>: no search,
 * $TNFS_CAR_MASKS not read. NULL when there is none or it does not read. */
uint8_t *car_mask_load_dir(const char *dir, const char name[4], int level, int width, int height);

#endif
