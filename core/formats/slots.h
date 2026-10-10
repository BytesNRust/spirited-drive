/* The track and car slots of the original executable (docs/modding/inventory.md §2). The EXE reaches every track,
 * car and sound bank through tables of names, so Level 1 modding replaces the files of an existing slot under
 * their own names and cannot add slots. This module only knows the names; the file system work is the studio's. */
#ifndef TNFS_SLOTS_H
#define TNFS_SLOTS_H

#include <stddef.h>

typedef enum { SLOT_TRACK, SLOT_CAR } SlotKind;

/* Case-insensitive lookup; `stem` receives the canonical upper-case stem (at least 16 bytes). 0 when unknown. */
int slot_find(SlotKind kind, const char *name, char *stem, size_t cap);

/* The canonical stems of a kind, NULL terminated. */
const char *const *slot_stems(SlotKind kind);

/* Whether a CD-relative path (any case, '/' separators) is one of the files the slot is made of by definition:
 *   track STEM: SIMDATA/MISC/STEM.TRI, SIMDATA/MISC/STEMINFO, SIMDATA/{NTRACKFM,ETRACKFM,GTRACKFM}/STEM_*.FAM
 *   car STEM:   SIMDATA/CARFAMS/STEM.CFM, SIMDATA/CARFAMS/MSTEM.CFM, SIMDATA/CARFAMS/STEM.PDN, SIMDATA/CARSPECS/STEM.PBS
 * Menu art, sound banks, dashboards and the like are named by the caller (they must exist in the data root). */
int slot_owns(SlotKind kind, const char *stem, const char *rel);

#endif
