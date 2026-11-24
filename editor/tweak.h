/* Edit tweak, step 1 (notes/diseno-studio-flujo-tweak-2026-11-24.md): the folder of a tweak of one of the game's own cars or tracks, and Apply to the game repo. The folder replicates data/ path by
 * path (car_masks/<folder>/, dash_masks/<STEM>/, track_light/<TRACK>.txt, track_masks/<TRACK>/) with a mod.ini of type=tweak; what is only Studio's (the PSDs, the colors, the target) is in studio/,
 * which the game never reads and a zip leaves out. Apply writes into a game repo's data/ and core/formats/car_paint.c what Copy to game repo writes today, through the same comparisons (masks by
 * picture, light by lines, colors into the car's row of the table). It never commits. Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_TWEAK_H
#define TNFS_TRACK_EDITOR_TWEAK_H

#include "car_export.h"
#include "formats/mod_ini.h"

#include <stddef.h>

typedef struct {
    int masks, dash, light, track_masks;   /* the files written into data/ */
    int colors;                            /* 1 when the car's row of core/formats/car_paint.c was rewritten */
    int colors_skipped;                    /* the card lists more colors than the table holds: how many (not compared) */
} TweakApplied;

/* the folder name of a tweak of `object` ("CY1", "Wagon", "LDiabl"): lower case, "-tweak" after it ("cy1-tweak") */
void tweak_id(const char *object, char *out, size_t cap);

/* Makes the tweak folder `dir` of one of the game's cars from its project's folders: `masks_dir` (the PNGs the game reads, may be NULL), `dash_dir` (a drivable car's, may be NULL), `psd_dir` (the
 * PSDs a person paints, may be NULL), and the card's colors (`paint_count` 0: none). An existing folder is made again from the project: its studio/ and files are replaced. 1, or 0 with `why`. */
int tweak_make_car(const char *dir, const ExportCar *car, const char *masks_dir, const char *dash_dir, const char *psd_dir, const ModPaint *paints, int paint_count, char *why, size_t cap);

/* The same for one of the 16 tracks: its one light file and its masks' PNGs (either may be NULL), its PSDs */
int tweak_make_track(const char *dir, const char *stem, const char *light_file, const char *masks_dir, const char *psd_dir, char *why, size_t cap);

/* Applies the tweak folder `dir` to the game repo whose data/ is `repo_data` (core/formats/car_paint.c is read beside it): the changed and new files are written, a file the repo already has the same
 * stays. 1 with `out` filled, or 0 with `why` (what was written before the failure stays written, as Copy's does). */
int tweak_apply(const char *dir, const char *repo_data, TweakApplied *out, char *why, size_t cap);

#endif
