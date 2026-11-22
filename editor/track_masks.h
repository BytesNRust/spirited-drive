/* The reflective masks of an original track's folder (dep103 phase B): art-source/masks/<name>.psd (the owner's, Photoshop) generate art-generated/track_masks/<name>.png (what the game
 * reads), never the other way. <name> is "slot_NN" or "scenery_NN" (track_tex.h). One source per texture: a PSD, or, for a texture with no PSD, its PNG.
 *
 * States of a texture's mask:
 *   TM_OK          the PNG is what the PSD makes
 *   TM_NEW         a PSD with painted marks and no PNG yet: generate makes it
 *   TM_STALE       the PSD is newer than its PNG and they differ: generate makes it again
 *   TM_EDITED      the PNG differs from the PSD's and is newer than it: someone changed the generated file by hand; it is NOT overwritten until Regenerate from PSD
 *   TM_PNG_SOURCE  a PNG with no PSD: the PNG is the source
 *   TM_EMPTY       a PSD with nothing painted in its mask layer: not a source; the PNG, if there is one, is kept (generate never overwrites it from an empty layer)
 *   TM_BAD         the PSD does not read (the reason is in `note`)
 *   TM_NONE        a texture of the track (known from `tex`) with no PSD and no PNG: it has no mask (the panel offers to make its PSD)
 * The rule that makes the PNG is tools/track_mask_from_psd.py's, in C (formats/psd_mask.h: psd_track_mask): white reflective, yellow a lamp's mark; the PNG is grey without lamps, RGBA with.
 * Pure C, POSIX files, no window. */
#ifndef TNFS_EDITOR_TRACK_MASKS_H
#define TNFS_EDITOR_TRACK_MASKS_H

#include <stddef.h>

#include "original_track.h"
#include "track_tex.h"

enum { TM_MAX = TRACK_TEX_MAX, TM_NOTE = 160 };

typedef enum { TM_OK = 0, TM_NEW, TM_STALE, TM_EDITED, TM_PNG_SOURCE, TM_EMPTY, TM_BAD, TM_NONE } TrackMaskState;

typedef struct {
    char name[TRACK_TEX_NAME];
    TrackMaskState state;
    int has_psd, has_png;
    int width, height;           /* the texture's (from `tex` when given), else the PSD's or the PNG's */
    int reflective, lamps;       /* texels the source marks (the PSD's, else the PNG's) */
    char note[TM_NOTE];
} TrackMaskRow;

typedef struct {
    TrackMaskRow row[TM_MAX];
    int count;
    int to_make;                 /* NEW + STALE: what generate writes */
    int edited;                  /* EDITED: kept until Regenerate */
} TrackMasks;

/* The rows of `dir` (an original track's folder): a row for every mask PSD (a backup, a name with a second dot, is not one) and every mask PNG, by name, and, when `tex` (NULL for none)
 * lists the track's textures, a TM_NONE row for each of them that has neither (the table lists every texture, masked or not). `tex` also gives the texture sizes. 1; 0 with `why` when the folder has neither art-source/masks nor art-generated/track_masks. */
int track_masks_scan(const char *dir, const TrackTexInfo *tex, int tex_count, TrackMasks *out, char *why, size_t why_cap);

/* Writes the PNG of every NEW and STALE row into art-generated/track_masks (made when missing); `written` gets how many. EDITED and EMPTY rows are never touched. 1, or 0 with `why`
 * (the files before the failing one are written). The rows are scanned again by the caller. */
int track_masks_generate(const char *dir, const TrackMasks *rows, int *written, char *why, size_t why_cap);

/* The newest modification time (seconds, fractions kept) of a mask PSD of `dir`; 0 when there is none. Stat only: what the Masks panel polls to find a PSD that was saved again. */
double track_masks_newest_psd(const char *dir);

/* Regenerate from PSD: the PNG of that one name made from its PSD even when it was edited by hand (not from an empty mask layer: refused, 0 with `why`). */
int track_masks_regenerate(const char *dir, const char *name, char *why, size_t why_cap);

/* The marks of a mask PNG as the game reads it (red 128 or more and opaque: reflective; red and green 128 or more with blue under 128: a lamp): `*marks` is w x h bytes of
 * TRACK_MASK_* (formats/psd_mask.h), malloc'd. 1, or 0 when the file does not read. */
int track_masks_load(const char *dir, const char *name, unsigned char **marks, unsigned *w, unsigned *h);

/* Publishing the masks (the light files are original_track.h's): appends to `plan` an item for each PNG of art-generated/track_masks, "track_masks/<STEM>/<name>.png" relative to
 * data/: same (the game's PNG marks the same texels), changed (with the texels gained and lost) or new. Compared as marks, so a PNG another encoder wrote is the same. 1, or 0 with `why`. */
int track_masks_publish_plan(const char *dir, const char *data_dir, const char *stem, OriginalPublish *plan, char *why, size_t why_cap);
/* Copies the changed and new mask items of `plan` into data/ (the folder made when missing); `written` is added to. 1, or 0 with `why`. */
int track_masks_publish(const char *dir, const char *data_dir, const OriginalPublish *plan, int *written, char *why, size_t why_cap);

/* a word for the state, for the panel and the tests: "ok", "new", "stale", "edited by hand", "PNG is the source", "no mask painted in the PSD", "unreadable PSD" */
const char *track_mask_state_text(TrackMaskState s);

#endif
