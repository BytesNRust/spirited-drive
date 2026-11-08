/* port (wipers): the wiper blades drawn over the 3D view (box in `pixels`), under the 2D layer: each an arm and a blade along it, the scene under them put through a darkening
 * table (the palette's index of each pixel to its near-black), the blade at the fraction wiper_fraction gives for the tick. Pure raster: no game state. */
#ifndef TNFS_WIPER_DRAW_H
#define TNFS_WIPER_DRAW_H

#include "game/race/wiper.h"

#include <stdint.h>

/* The blades of `w` (in the box's pixels, from its left top) at `tick`. `dark` maps a pixel to the one drawn over it; pixels outside the box are left alone. A blade at the park is drawn too. */
void wiper_draw(uint8_t *pixels, int32_t pitch, const int32_t box[4], const WiperPass *w, uint32_t tick, const uint8_t dark[256]);

#endif
