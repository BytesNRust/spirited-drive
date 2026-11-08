/* port (rally_spray): the colour of the spray a car throws up in a rally's rain, taken from the road under it: the water is a lightening of the picture (rain_spray.h's shapes through
 * the weather's tables), on a dirt or gravel road it is a darker mist of the road's own colour (grey on gravel, brown on dirt) and on snow it is white. Pure: a colour in, a table out; the
 * road's colour is the pixels the road was drawn with, so no new colour is stored anywhere. */
#ifndef TNFS_SPRAY_TINT_H
#define TNFS_SPRAY_TINT_H

#include <stdint.h>

/* The colour the spray takes from a road of mean colour `road` (8-bit RGB): the road, darker (a wet road's mist is dirty), keeping its hue; a bright road (snow) gives a white. */
void spray_tint_target(const uint8_t road[3], uint8_t out[3]);

/* A 256-entry table for the 8-bit palette `pal` (768 bytes RGB): each index goes to the palette entry nearest to its colour moved `percent` (0..100) of the way to `tint`. The index 0xFF
 * (transparent) and an unused (black) palette are left as they are. */
void spray_tint_table(const uint8_t pal[768], const uint8_t tint[3], int percent, uint8_t table[256]);

/* A key that is the same for colours that differ by a few steps (the road's grain), to keep the tables made. */
uint32_t spray_tint_key(const uint8_t colour[3]);

#endif
