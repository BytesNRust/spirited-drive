/* port: a defect of the CD's data that the race shows as it is on the original too. CL1_R01.FAM (Coastal's rally art)
 * has the palette of CL1_M01.FAM (the mirror's) but for colour 2: pure green where the mirror's has its rock grey
 * (105, 101, 101). The rock textures (26834 pixels of the opaque ones) use colour 2, so the cliffs come out spotted
 * with bright green. No other track's FAM has green there. */
#ifndef TNFS_TRACK_PALETTE_REPAIR_H
#define TNFS_TRACK_PALETTE_REPAIR_H

#include <stdint.h>

enum { TRACK_PALETTE_BYTES = 0x300 };

/* A track palette as 6 bits a channel: colour 2 of (0, 63, 0) becomes (26, 25, 25), the mirror FAM's grey; 1 when it
 * did, 0 for anything else (the palette untouched). */
int track_palette_repair(uint8_t pal6[TRACK_PALETTE_BYTES]);

#endif
