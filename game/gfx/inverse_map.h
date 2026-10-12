/* The inverse colour map 00473140 looks colours up in when 004c5ff4 is set. */
#ifndef TNFS_INVERSE_MAP_H
#define TNFS_INVERSE_MAP_H

#include <stdint.h>

enum {
    INVERSE_MAP_BITS = 5,                /* 00474578 passes 5 */
    INVERSE_MAP_SIZE = 1 << (3 * INVERSE_MAP_BITS),
    INVERSE_MAP_COLOURS = 256
};

/* 00473d80(256, entries, 5, ...): `entries` are 256 x 4 bytes (B, G, R, x,
 * as 0052f178); each enabled one, in order, claims the cells (index
 * r << 10 | g << 5 | b, cell centres 8 x + 4) it is nearer to than what
 * holds them, by Thomas's incremental scan (not an exact nearest search).
 * Cells no entry claims keep what `map` held. 0 when the distance buffer
 * cannot be had (map untouched). */
int inverse_map_build(const uint8_t entries[INVERSE_MAP_COLOURS * 4], const uint8_t enabled[INVERSE_MAP_COLOURS],
                      uint8_t map[INVERSE_MAP_SIZE]);

#endif
