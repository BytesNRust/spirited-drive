/* port (the PS1 look's shadow, port.cfg psx_shadow): the footprint under each car as the PS1 shows it, a soft dark shape that is weaker where
 * the segment's light is low (tunnels, bridges), in place of the exe's one hard-edged quad. The quad's corners are the exe's (car_shadow.h's
 * screen[8]); only the way it is shaded changes. The darkening is a remap of the surface (the exe's 004c6604 shade table); a table half
 * way to it is made here for the rim and for the weaker levels. With the setting off none of this runs and the
 * frames are the exe's. */
#ifndef TNFS_SOFT_SHADOW_H
#define TNFS_SOFT_SHADOW_H

#include "game/gfx/quad.h"

#include <stdint.h>

/* how much shadow a car's light gets */
typedef enum { SOFT_SHADOW_DARK, SOFT_SHADOW_MID, SOFT_SHADOW_BRIGHT } SoftShadowLevel;

/* The level for the light at a car's node (psx_light_node: 0 black .. 255 full): bright, near the exe's shadow with a soft rim; mid, the
 * whole quad at half the darkening; dark, a smaller quad at half the darkening, so it does not stack into black. */
SoftShadowLevel soft_shadow_level(uint8_t light);

/* `half`: for each palette entry, the entry nearest the colour half way between it and its shade table's (`full`) entry. */
void soft_shadow_half(const uint8_t palette[768], const uint8_t full[256], uint8_t half[256]);

/* The quad's four corners (x, y pairs) each moved `px` pixels in x and in y towards the quad's centre, no further than the centre. */
void soft_shadow_shrink(const int32_t in[8], int32_t out[8], int px);

/* The shadow of `level` on `screen`'s quad (`full`: the exe's shade table, `half` its half-way one): bright, the exe's darkening on a core
 * that is the quad shrunk, a rim of `half` round it; mid, the quad in `half`; dark, the shrunken quad in `half`. */
void soft_shadow_draw(const QuadTarget *target, const uint8_t full[256], const uint8_t half[256], SoftShadowLevel level, const int32_t screen[8]);

#endif
