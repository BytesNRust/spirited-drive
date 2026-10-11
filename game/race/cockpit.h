/* The in-car dashboard (the DASH / GDASH files, 0041f9a0 loads them): its
 * bank 004f9740 and 6-bit palette 004f973c. */
#ifndef TNFS_COCKPIT_H
#define TNFS_COCKPIT_H

#include "game/race/race_palette.h"

#include <stdint.h>

enum {
    COCKPIT_COLOUR_FIRST = 0, COCKPIT_COLOUR_COUNT = 0x30
};

/* 0041f988: queues the dashboard's colours 0..0x2f (00446f90). */
void cockpit_queue_palette(RacePalette *pal, const uint8_t *palette);

#endif
