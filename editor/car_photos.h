/* The showcase photos of a car project (the owner's Car Showcase gallery): the pictures the card's `photos=` names, in order, each a PNG in the piece's root. Studio
 * lists them, copies a chosen picture into the piece under a safe name, and removes the files it made that the card no longer names. Pure C, no window. */
#ifndef TNFS_TRACK_EDITOR_CAR_PHOTOS_H
#define TNFS_TRACK_EDITOR_CAR_PHOTOS_H

#include "formats/mod_ini.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* `value` (the card's photos= text: names between commas, spaces round them dropped) as names, at most `cap`: how many */
int car_photos_split(const char *value, char (*names)[MOD_INI_PHOTO_NAME_MAX + 1], int cap);
/* `n` names as the card's text, "a.png,b.png" (cut, still a string, when `cap` is short) */
void car_photos_join(char (*names)[MOD_INI_PHOTO_NAME_MAX + 1], int n, char *out, size_t cap);

/* A file name for the picture at `source`, one mod_ini_photo_name_ok takes: the source's name lower-cased, anything but a-z 0-9 - _ . made one dash, ending .png, at most
 * MOD_INI_PHOTO_NAME_MAX; not one of the `n` names in `taken` (-2, -3, ... is added) and not one of the game's own (card, rival, gallery, preview, car_envmap). 1 with `out`. */
int car_photos_safe_name(const char *source, char (*taken)[MOD_INI_PHOTO_NAME_MAX + 1], int n, char *out, size_t cap);

/* The picture at `source` copied into `piece` as `name`, staged as name.tmp and renamed: it must be a readable PNG of CAR_PIECE_PHOTO_MIN to MAX a side (the showcase's
 * limits). 1, or 0 with `why` (English, for the person; nothing is left behind). */
int car_photos_import(const char *source, const char *piece, const char *name, char *why, size_t why_cap);

/* A photo Studio copies in waits in the piece as name.new (not a photo's name, so an unsaved one never trips the piece's check); car_photos_commit renames each such file `listed`
 * (the card's photos= text) names to its own name once the card is saved: how many. 0 with nothing to do. */
int car_photos_commit(const char *piece, const char *listed);

/* Of the `*made_n` names Studio made in `piece` (`made`), removes the files (name and name.new) of those `listed` (photos= and photo_small=, as saved) does not name, and drops
 * them from `made`; one that cannot be removed stays in `made`. Other files are never touched. How many were removed. */
int car_photos_sweep(const char *piece, char (*made)[MOD_INI_PHOTO_NAME_MAX + 1], int *made_n, const char *listed);

/* the size of the PNG `name` (or its name.new, not committed yet) in `piece`, read from its header: 1 with *w and *h, 0 when there is no such readable picture */
int car_photos_size(const char *piece, const char *name, uint32_t *w, uint32_t *h);

#ifdef __cplusplus
}
#endif

#endif
