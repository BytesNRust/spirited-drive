/* port: Lunar Springs, the moon version of Rusty Springs' rally, a track that was cut: the PC CD still holds its
 * parts (the moon's textures NTRACKFM/TR8_R01.FAM, its Control Central picture FRONTEND/GART/CONTROL/MOON.QFS and its
 * voices FRONTEND/SPEECH/LUNARWEL.EAS and LUNARGOL.EAS) but not the moon's own geometry (no MISC/TR8.TRI), and the PS1
 * disc has no moon file at all. So the rally keeps Rusty's geometry (MISC/TR5.TRI, which is Rusty's own) and only the
 * moon's art is used. With extras.cfg's lunar_springs on, the rally on Rusty Springs runs those textures and shows
 * that picture; nothing else changes, and the records stay Rusty's. */
#ifndef TNFS_LUNAR_SPRINGS_H
#define TNFS_LUNAR_SPRINGS_H

#include <stddef.h>

/* the setup's track index of Rusty Springs (race_sim_track_index's 3, the rally's "tr5") and its Control Central
 * art entry, the rally's springs ("springsr" in central_draw.c's TRACK_NAMES: the rally base 19 plus 3) */
enum { LUNAR_TRACK_RUSTY = 3, LUNAR_ART_SPRINGSR = 22 };

/* 1 when the race is Rusty Springs' rally (track `track` with variant_r set) and `lunar` is on: the moon's race. */
int lunar_springs_active(int track, int variant_r, int lunar);

/* 1 when the moon's race also takes the moon's gravity and grip (track_gravity.h): the moon's race with extras.cfg's
 * lunar_gravity on, 0 otherwise. */
int lunar_gravity_active(int track, int variant_r, int lunar, int lunar_gravity);

/* 1 when the race is Rusty Springs' rally (track `track` with variant_r set) and `lunar` is on: the FAM to read is
 * then "<data_root>/SIMDATA/NTRACKFM/TR8_R01.FAM", written to `out` (size bytes). 0 leaves `out` alone. */
int lunar_springs_fam(const char *data_root, int track, int variant_r, int lunar, char *out, size_t size);

/* The moon's scenery bitmaps that carry text (the EA SPACE sign): the CD has no mirror art for the moon, so a mirrored
 * Lunar Springs flips them to read (fam_flip_named, the FAM's root child LUNAR_SIGNS_BANK). Names only, no pixels. */
enum { LUNAR_SIGNS_BANK = 1, LUNAR_SIGN_COUNT = 1 };
extern const char LUNAR_SIGNS[LUNAR_SIGN_COUNT][4];

/* The moon's road seen from far ("01A4", the variant the race draws at a distance) is a crater, while its near road
 * ("01A0") is the plain ground under the start's gantry; Rusty's own far road is its near one's ground. The far road
 * takes the near one's ground (fam_fill_named, the near one's top quarter, the gantry, left out). Names only. */
enum { LUNAR_ROAD_BANK = 0, LUNAR_ROAD_GANTRY_QUARTERS = 1 };
extern const char LUNAR_FAR_ROAD[4], LUNAR_NEAR_ROAD[4];

/* 1 when Control Central's track art entry `art` is Rusty's rally picture and `lunar` is on: the moon's picture is
 * written to `out` as a name for the art loader (relative to frontend/art/ or frontend/gart/, "../gart/control/
 * moon.qfs"). 0 leaves `out` alone. */
int lunar_springs_picture(int art, int lunar, char *out, size_t size);

#endif
