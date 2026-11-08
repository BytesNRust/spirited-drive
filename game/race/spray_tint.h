/* port (rally_spray): the colour of the spray a car throws up in a rally's rain, taken from the road under it: the water is a lightening of the picture (rain_spray.h's shapes through
 * the weather's tables), on a dirt or gravel road it is a darker mist of the road's own colour (grey on gravel, brown on dirt) and on snow it is white. Pure: a colour in, a table out; the
 * road's colour is the pixels the road was drawn with, so no new colour is stored anywhere. */
#ifndef TNFS_SPRAY_TINT_H
#define TNFS_SPRAY_TINT_H

#include <stddef.h>
#include <stdint.h>

/* The colour the spray takes from a road of mean colour `road` (8-bit RGB): the road, darker (a wet road's mist is dirty), keeping its hue; a bright road (snow) gives a white. */
void spray_tint_target(const uint8_t road[3], uint8_t out[3]);

/* A 256-entry table for the 8-bit palette `pal` (768 bytes RGB): each index goes to the palette entry nearest to its colour moved `percent` (0..100) of the way to `tint`. The index 0xFF
 * (transparent) and an unused (black) palette are left as they are. */
void spray_tint_table(const uint8_t pal[768], const uint8_t tint[3], int percent, uint8_t table[256]);

/* A key that is the same for colours that differ by a few steps (the road's grain), to keep the tables made. */
uint32_t spray_tint_key(const uint8_t colour[3]);

/* port (spray_cd_smoke): the CD's smoke sprite ('smok') drawn as a mist, the exe's additive way (puffs.h: the sprite's grey lifts what is under it) but through tables of ours, in the colour of the
 * surface and more transparent. The sprite's texels are turned into SPRAY_MIST_LEVELS levels of grey and each level is a table (spray_add_table) for the surface's colour. */
enum { SPRAY_MIST_LEVELS = 8, SPRAY_MIST_WATER = 0, SPRAY_MIST_MUD = 1, SPRAY_MIST_SNOW = 2 };

/* What the road under the spray is: snow (a bright road), mud / dust (a road with a hue: dirt, sand) or water (a grey one: asphalt, gravel, wet or not). */
int spray_mist_kind(const uint8_t road[3]);

/* The colour the mist adds: white (a touch cool) for water, white for snow, the road's own hue lifted toward beige for mud and dust. */
void spray_mist_colour(const uint8_t road[3], uint8_t out[3]);

/* A 256-entry table for the 8-bit palette `pal`: each index goes to the palette entry nearest to its colour plus `colour` x amount / 255 (each channel, to 255), as the exe's puff blend adds the
 * sprite's grey. The index 0xFF (transparent) is left as it is; an amount of 0 is the identity. */
void spray_add_table(const uint8_t pal[768], const uint8_t colour[3], int amount, uint8_t table[256]);

/* The shape `shape` (8-bit, type 0x7b: width word +4, height +6, texels from +0x10) as levels: each texel becomes 0 (nothing: the transparent index, or a black one) to SPRAY_MIST_LEVELS by the
 * grey `grey[texel]` it has on the screen; the header is copied. `out` must hold 16 + width x height bytes (`out_size`). Returns 1, or 0 for no room or a shape of another type. */
int spray_mist_shape(const uint8_t *shape, const uint8_t grey[256], uint8_t *out, size_t out_size);

#endif
