/* tnfs-studio car texture add: a bitmap of the car's own added to a car piece (dep71 follow-up 2,
 * notes/dep71-texture-add-plan.md; formats/car_texture_add.h).
 *
 *   tnfs-studio car texture add PIECE_DIR NAME FILE.png [--low] [--label LABEL]
 *
 * NAME (4 characters a-z 0-9, not one the bank has nor one the race looks up) gets the picture, fitted to the bank's palette as car
 * import fits one, after the bank's last entry: the high model's, or with --low the far model's (its own palette). The model gets a
 * texture reference to it (label LABEL, default NAME) with no face on it yet; the command says the material to give faces in
 * the 3D editor, t<index>_<label>. Unpack the piece again for the 3D editor to list it; `car import --raw` keeps it. */
#ifndef TNFS_STUDIO_CAR_TEXTURE_H
#define TNFS_STUDIO_CAR_TEXTURE_H

#include "formats/exchange.h"

/* argv[0] is "car", argv[1] "texture". 0 with `err` filled on failure. */
int car_texture_command(int argc, char **argv, ExchangeError *err);

#endif
