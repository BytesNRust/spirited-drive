/* New car (dep102): a car of the workspace made from one of the eight cars you drive, as a kit in the current layout (formats/car_kit_paths.h). It is `car new` (the
 * template's CFM, PDN and PBS under a new stem, a card with sound_from and dash_from the template's word, the port's masks) followed by `car kit`, so the kit is what
 * `tnfs-studio car new` then `car kit` make by hand. Needs tnfs_tools. */
#ifndef TNFS_TRACK_EDITOR_CAR_NEW_KIT_H
#define TNFS_TRACK_EDITOR_CAR_NEW_KIT_H

#include <stddef.h>

/* The stem for car `id`: its letters and digits in upper case (up to 8), an X in front when that is a car of the game's or empty, and two digits when another kit in
 * `cars_dir` (the workspace's cars/) already has it. 1, or 0 when no stem is free. */
int car_new_stem(const char *cars_dir, const char *id, char *stem, size_t cap);

/* Makes <cars_dir>/<id> from `template_word` (viper, 911 ... or a stem on the CD): `cd` is the game's files (SIMDATA/ under it), `port` the data folder with car_masks/
 * and dash_masks/. `name` is the card's name ("" for the id). The folder must not be there yet: the kit is made beside as "<id>.making" and renamed when whole.
 * 1 when made, `kit` the folder; 0 with `why` in words, nothing left behind. */
int car_new_kit(const char *cd, const char *port, const char *cars_dir, const char *template_word, const char *name, const char *id, char *kit, size_t kit_cap, char *why, size_t why_cap);

#endif
