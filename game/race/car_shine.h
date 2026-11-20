/* The body shine's tables live in core now (raster/car_shine.h: the Studio's 3D view draws with them); this keeps the old include path and gives
 * the game's own call, on the screen palette's usable and emissive entries (game/sys/palette.h). */
#ifndef TNFS_CAR_SHINE_H
#define TNFS_CAR_SHINE_H

#include "raster/car_shine.h"

/* car_shine_tables with palette_usable() and palette_emissive() */
void car_envmap_tables(const uint8_t palette[768], uint8_t paint[CAR_SHINE_LEVELS][256], uint8_t glass[CAR_SHINE_LEVELS][256]);

#endif
