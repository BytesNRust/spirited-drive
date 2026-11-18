/* A template TRI of our own (dep59 editor, phase 1a): what track_gen takes from a CD track, made in code, so a generated track carries
 * no byte of the CD and a package of it can be shared. A straight stage of TRACK_TEMPLATE_SEGMENTS segments, no scenery, a symmetric
 * cross-section (road, shoulder, verge, two bands of grass) and the texture slots below, which the generic FAM provides:
 * terrain SHPI n of the FAM holds the slots n * 3 + A, B, C, and the TRI's texture byte is that slot number (game/race/track_art.h). */
#ifndef TNFS_TRACK_TEMPLATE_H
#define TNFS_TRACK_TEMPLATE_H

#include "formats/exchange.h"

#include <stddef.h>
#include <stdint.h>

enum {
    TRACK_TEMPLATE_SEGMENTS = 12,     /* track_gen takes its cross-section and brightness from strip 10, so 11 are the least */
    TRACK_TEMPLATE_HALF_WIDTH = 40,   /* the road's verge bytes: eighths of a unit, 5 units each side of the centre */
    TRACK_TEMPLATE_BARRIER = 64,      /* the barrier bytes: 8 units from the centre */
    TRACK_GENERIC_BANKS = 4,          /* terrain SHPIs the FAM must have: 0 road, 1 shoulder, 2 verge, 3 grass (a stage needs two at least) */
    TRACK_GENERIC_SLOT_ROAD = 0 * 3,
    TRACK_GENERIC_SLOT_SHOULDER = 1 * 3,
    TRACK_GENERIC_SLOT_VERGE = 2 * 3,
    TRACK_GENERIC_SLOT_GRASS = 3 * 3
};

/* The template as a TRI file: 1 and *out (malloc'd, *out_len bytes), the same bytes every time; 0 with err when it cannot be assembled. */
int track_template_generic(uint8_t **out, size_t *out_len, ExchangeError *err);

#endif
