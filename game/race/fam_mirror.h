/* port: the art of a race the CD holds no FAM for: both variants on (the mirror's geometry, the rally's _R01 art), or
 * a variant at another time of day (the _T01 art; extras.cfg's variant_time). The CD's mirror art (_M01) holds the
 * signs flipped so they read on the mirrored track; the rally and evening art were never made for it, and there is
 * no evening rally art at all. */
#ifndef TNFS_FAM_MIRROR_H
#define TNFS_FAM_MIRROR_H

#include <stddef.h>
#include <stdint.h>

/* Flips in place every bitmap of `target` (a FAM) whose namesake at the same place in `mirror` is the exact left-right
 * flip of `reference`'s (which may be `target` itself), and only those: the rest is kept. Returns how many were
 * flipped; a FAM that cannot open, or a place the three do not share at one size, is left as it is. */
int fam_mirror_signs(uint8_t *target, size_t target_size, const uint8_t *reference, size_t reference_size,
                     const uint8_t *mirror, size_t mirror_size);

/* Puts the rally's own terrain and scenery bitmaps (those that differ from the `day` art's) into `timed` (the _T01
 * art), their colours taken through the change from day to that time: per channel, fitted on the pixels the day and
 * timed art share, then the nearest of the timed palette (0xff, transparent, stays). Returns how many were put. */
int fam_time_rally(uint8_t *timed, size_t timed_size, const uint8_t *day, size_t day_size, const uint8_t *rally,
                   size_t rally_size);

#endif
