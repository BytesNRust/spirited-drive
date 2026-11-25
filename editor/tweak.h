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
    int skipped;                           /* the files the repo has changes of its own in, left as they are (tweak_apply_skipping) */
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

/* What a tweak holds against the game: each file of the tweak folder that Apply would write, as the game's data/ `base_data` has it (same, changed with its size, new). The colors are one row. */
typedef enum { TWEAK_SAME = 0, TWEAK_CHANGED, TWEAK_NEW } TweakState;
enum { TWEAK_FILES = 160, TWEAK_NAME = 96, TWEAK_NOTE = 96 };
typedef struct {
    char file[TWEAK_NAME];   /* the path in the tweak folder ("track_light/CY1.txt", "car_masks/Wagon/side_mask.png", "colors") */
    TweakState state;
    char note[TWEAK_NOTE];   /* "12 lines", "140 texels", "" */
    int own;                 /* the repo's file is neither the game's it was made from nor the tweak's: it has changes of its own (studio/base.txt knows the first) */
} TweakFile;
typedef struct {
    TweakFile item[TWEAK_FILES];
    int count, changed, same, own;
    char target[16];         /* "car" or "track" */
    char object[64];         /* the car's masks folder or the track's stem */
    char summary[160];       /* one line: "changed 3 masks, colors", "same as the game" */
} TweakStatus;

/* The status of the tweak folder `dir` against `base_data`. 1, or 0 with `why` (not a tweak of Studio, or the game's folder is not there to compare with). */
int tweak_status(const char *dir, const char *base_data, TweakStatus *out, char *why, size_t cap);

/* The fingerprints of the game's files the tweak carries a file for, as `base_data` has them now, in studio/base.txt ("-" for a file it has not): what lets Apply tell a repo's own changes from
 * the game's. Kept as it is when it is there (the first state is the one that counts). 1, or 0 with `why`. */
int tweak_base_write(const char *dir, const char *base_data, char *why, size_t cap);

/* Apply that leaves alone the files whose repo copy has changes of its own (TweakStatus.own): `skipped` counts them. With none it is tweak_apply. */
int tweak_apply_skipping(const char *dir, const char *repo_data, TweakApplied *out, char *why, size_t cap);

/* The checks that need no game: mod.ini is a type=tweak, studio/target.txt names an object, every mask and light file is for that object and reads. 1 with `report` "ok", or 0 with the first problem. */
int tweak_check(const char *dir, char *report, size_t cap);

/* Pack zip: Check (tweak_check and tweak_check_masks), then the files that differ from `base_data` (not studio/, not the colors, not what equals the game) go
 * through `mod pack` into `zip` (`force` replaces one). 0 with `report` saying why; 1 with the report saying what went in. A track light with a tunnel or snow fact is refused naming the line. */
int tweak_check_masks(const char *dir, char *report, size_t cap);   /* the masks hold class values only and no PSD is there */
int tweak_pack(const char *dir, const char *base_data, const char *zip, int force, char *report, size_t cap);

#endif
