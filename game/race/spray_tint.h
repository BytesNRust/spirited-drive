/* port: a palette table that moves each colour toward a tint, for the spray's colours (spray_v4_draw.c) and the weather's (weather_hooks.c). Pure: a colour in, a table out. */
#ifndef TNFS_SPRAY_TINT_H
#define TNFS_SPRAY_TINT_H

#include <stdint.h>

/* A 256-entry table for the 8-bit palette `pal` (768 bytes RGB): each index goes to the palette entry nearest to its colour moved `percent` (0..100) of the way to `tint`. The index 0xFF
 * (transparent) and an unused (black) palette are left as they are. */
void spray_tint_table(const uint8_t pal[768], const uint8_t tint[3], int percent, uint8_t table[256]);

#endif
