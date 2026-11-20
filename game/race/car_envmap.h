/* The envmap lives in core now (raster/car_envmap.h: the Studio's 3D view draws with it); this keeps the old include path and adds the game's
 * own image: the file lookup (TNFS_CAR_ENVMAP, data/car_envmap.png) and its cache over the mods in effect. */
#ifndef TNFS_CAR_ENVMAP_H
#define TNFS_CAR_ENVMAP_H

#include "raster/car_envmap.h"

/* the image, made on the first call and again when the mods in effect change (CAR_ENVMAP_SIZE^2 bytes, mod_set.h): car_envmap_build's */
const uint8_t *car_envmap_image(void);

/* the image the lookup chain gives into `image` (CAR_ENVMAP_SIZE squared bytes): the first of TNFS_CAR_ENVMAP (a full path), data/car_envmap.png
 * (the working directory) and TNFS_SOURCE_DIR/data/car_envmap.png that reads (car_envmap_file), else the procedural image (car_envmap_make) */
void car_envmap_build(uint8_t *image);

#endif
