/* What an exported project's masks have changed from the game's own (dep59 car editor): each `*_mask.png` of the project's car_masks/<folder>/ compared with the same file in the
 * game's data/car_masks/<folder>/ that the export started from: unchanged, changed (how many texels, which classes gained and lost them), new (only the project has it) or missing
 * (only the game has it). Read only. Masks are compared as classes (car_mask_codec.h), so a file written again by another PNG encoder with the same texels is unchanged. */
#ifndef TNFS_TRACK_EDITOR_MASK_CHANGES_H
#define TNFS_TRACK_EDITOR_MASK_CHANGES_H

#include "mask_edit.h"

#include <stddef.h>

enum { MC_MAX_FILES = 64, MC_NAME = 64, MC_NOTE = 160 };

typedef enum { MC_UNCHANGED = 0, MC_CHANGED, MC_NEW, MC_MISSING } MaskChangeKind;

typedef struct {
    char file[MC_NAME];
    MaskChangeKind kind;
    int texels;                      /* changed: the texels whose class differs (all of them when the size differs) */
    int gained[ME_CLASSES];          /* changed, same size: the texels each class (the legend's order) has now that it had not */
    int lost[ME_CLASSES];            /* and the texels it had that are another class now */
    char note[MC_NOTE];              /* changed with another size: "156x55 -> 160x55"; "" otherwise */
} MaskChange;

typedef struct {
    MaskChange item[MC_MAX_FILES];   /* by file name */
    int count;
    int unchanged, changed, added, missing;   /* how many of each kind */
} MaskChanges;

/* Compares the masks in `project` with those in `game` (folders). 1 with `out` filled; 0 with `why` (the project's folder cannot be read, or the game's folder is not there: the
 * comparison needs it). */
int mask_changes_compare(const char *project, const char *game, MaskChanges *out, char *why, size_t why_cap);

/* The list for the owner to paste: the folder, then a line for each changed, new and missing file (a changed one with its texel count and the classes that gained and lost texels),
 * and how many are unchanged. `folder` names the car's masks folder, "Wagon". */
void mask_changes_text(const MaskChanges *c, const char *folder, char *out, size_t cap);

#endif
