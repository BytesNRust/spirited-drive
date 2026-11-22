/* Check dash (dep98): which of a car's masks are older than the owner's PSDs they come from, by file times (the rule of tools/masks_status.py): each
 * <tex>.psd (and <tex>_lo.psd) of the PSD folder against the project's car_masks/<folder>/<tex>_mask.png, and dash.psd against the dash_masks/<car>/dash_mask.png.
 * A PNG that is missing or older than its PSD is stale; one copied after its PSD never is. A backup (a name with a second dot: dash.before-wipers.psd) is not a
 * PSD of the car. Names match without case. Read only. POSIX only, no window. */
#ifndef TNFS_TRACK_EDITOR_PSD_STALE_H
#define TNFS_TRACK_EDITOR_PSD_STALE_H

#include <stddef.h>

enum { STALE_MAX = 24, STALE_NAME = 64 };

typedef enum { STALE_OK = 0, STALE_NEWER, STALE_MISSING } StaleState;

typedef struct {
    char psd[STALE_NAME];   /* "frnt.psd" as the PSD folder spells it */
    char png[STALE_NAME];   /* "frnt_mask.png", "dash_mask.png" */
    int dash;               /* the dash's, not a body texture's */
    int edited;             /* its PNG was edited by hand since Studio made it (psd_manifest.h): the caller sets it, psd_stale_check never does */
    long psd_time;          /* the PSD's modification time: a PSD saved again is a new state, whatever the PNG says */
    StaleState state;
} StaleItem;

typedef struct {
    int count;              /* the PSDs looked at */
    int stale;              /* how many are not STALE_OK */
    StaleItem item[STALE_MAX];
} StaleReport;

/* `piece` is the project's folder that holds car_masks/ and dash_masks/, `folder` the masks folder name below car_masks/; `drivable` a car you drive (it has a dash).
 * 1 with `out` filled (a PSD folder with no PSD is 0 items); 0 when the PSD folder cannot be read. */
int psd_stale_check(const char *psd_dir, const char *piece, const char *folder, int drivable, StaleReport *out);

/* The same for a car kit (art-source/masks, art-generated/car_masks and art-generated/dash_masks): `masks` and `dash` are the folders the PNGs are in, flat (no folder of the car's name below them). */
int psd_stale_check_flat(const char *psd_dir, const char *masks, const char *dash, int drivable, StaleReport *out);

/* the stale ones in a sentence for the person: "dash.psd is newer than dash_mask.png; frnt.psd has no frnt_mask.png"; "" when none is stale */
void psd_stale_text(const StaleReport *r, char *out, size_t cap);

#endif
