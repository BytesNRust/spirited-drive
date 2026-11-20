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
    TRACK_GENERIC_BANKS = 11,         /* terrain SHPIs the FAM must have: 0 road, 1 shoulder, 2 verge, 3 grass, 4 the tarmac shoulder (letter A of
                                         each), 5..9 anything, and 10 the fence (its letter C): the fence's texture is the slot 32 + the fence byte's low 5 bits */
    TRACK_GENERIC_FENCE_SLOT = 32,
    TRACK_TEMPLATE_FENCE = 0xE0,      /* the strips' fence byte: bits 7 and 6: a fence each side; 0x20: the low one, and the texture slot TRACK_GENERIC_FENCE_SLOT (32 + the low 5 bits: 0) */
    TRACK_GENERIC_SLOT_ROAD = 0 * 3,
    TRACK_GENERIC_SLOT_SHOULDER = 1 * 3,
    TRACK_GENERIC_SLOT_VERGE = 2 * 3,
    TRACK_GENERIC_SLOT_GRASS = 3 * 3,
    TRACK_GENERIC_SLOT_TARMAC = 4 * 3,  /* a tarmac shoulder (phase 2 step 2b): the road's asphalt without its lines. An art of the author's own draws its bank 4 */
    TRACK_GENERIC_SLOT_LANES = 6 * 3   /* the two-lane half of a 1+2 or 2+1 stretch (phase 2 step 4): the yellow centre inside, a dashed line in the middle, the
                                          edge line outside, one strip of 10 m; an author's art draws its bank 6 */
};

/* The track's INFO file (the horizon, the fog and the time-of-day colours the game reads: text, one value a line) made for this template and
 * its FAM: *len bytes of static text. */
const char *track_template_info(size_t *len);

/* Whether `fam` (a track art file) has what the template's slots need: TRACK_GENERIC_BANKS terrain banks, and the fence's letter C in the last.
 * 1 when it does; 0 with the reason in err. */
int track_template_check_fam(const uint8_t *fam, size_t size, ExchangeError *err);

/* The template as a TRI file: 1 and *out (malloc'd, *out_len bytes), the same bytes every time; 0 with err when it cannot be assembled. */
int track_template_generic(uint8_t **out, size_t *out_len, ExchangeError *err);

#endif
