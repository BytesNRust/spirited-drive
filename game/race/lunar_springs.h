/* port: Lunar Springs, the cut moon version of Rusty Springs' rally (PS1-only in retail). The CD still holds its
 * parts: the rally track's models (MISC/TR5.TRI, which is Rusty's own), the moon's textures (NTRACKFM/TR8_R01.FAM)
 * and its Control Central picture (FRONTEND/GART/CONTROL/MOON.QFS). With extras.cfg's lunar_springs on, the rally on
 * Rusty Springs runs those textures and shows that picture; nothing else changes, and the records stay Rusty's. */
#ifndef TNFS_LUNAR_SPRINGS_H
#define TNFS_LUNAR_SPRINGS_H

#include <stddef.h>

/* the setup's track index of Rusty Springs (race_sim_track_index's 3, the rally's "tr5") and its Control Central
 * art entry, the rally's springs ("springsr" in central_draw.c's TRACK_NAMES: the rally base 19 plus 3) */
enum { LUNAR_TRACK_RUSTY = 3, LUNAR_ART_SPRINGSR = 22 };

/* 1 when the race is Rusty Springs' rally (track `track` with variant_r set) and `lunar` is on: the FAM to read is
 * then "<data_root>/SIMDATA/NTRACKFM/TR8_R01.FAM", written to `out` (size bytes). 0 leaves `out` alone. */
int lunar_springs_fam(const char *data_root, int track, int variant_r, int lunar, char *out, size_t size);

/* The moon's scenery bitmaps that carry text (the EA SPACE sign): the CD has no mirror art for the moon, so a mirrored
 * Lunar Springs flips them to read (fam_flip_named, the FAM's root child LUNAR_SIGNS_BANK). Names only, no pixels. */
enum { LUNAR_SIGNS_BANK = 1, LUNAR_SIGN_COUNT = 1 };
extern const char LUNAR_SIGNS[LUNAR_SIGN_COUNT][4];

/* 1 when Control Central's track art entry `art` is Rusty's rally picture and `lunar` is on: the moon's picture is
 * written to `out` as a name for the art loader (relative to frontend/art/ or frontend/gart/, "../gart/control/
 * moon.qfs"). 0 leaves `out` alone. */
int lunar_springs_picture(int art, int lunar, char *out, size_t size);

#endif
