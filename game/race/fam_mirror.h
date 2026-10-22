/* port: the art of a race with both variants on (the mirror's geometry, the rally's _R01 art). The CD's mirror art
 * (_M01) holds the signs flipped so they read on the mirrored track; the rally art was never made for it. */
#ifndef TNFS_FAM_MIRROR_H
#define TNFS_FAM_MIRROR_H

#include <stddef.h>
#include <stdint.h>

/* Flips in place every bitmap of `rally` (a FAM) whose namesake at the same place in `mirror` is its exact left-right
 * flip, and only those: the rest of the rally's art is kept. Returns how many were flipped; a FAM either side cannot
 * open, or a place the two do not share, is left as it is. */
int fam_mirror_signs(uint8_t *rally, size_t rally_size, const uint8_t *mirror, size_t mirror_size);

#endif
