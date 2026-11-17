/* tnfs-studio car import: a model made in any modeller put into a car piece of car new (dep59 tools B2; formats/car_import.h,
 * docs/modding/car-model.md "Importing a model").
 *
 *   tnfs-studio car import PIECE_DIR --obj FILE.obj [--low FILE.obj] [--textures DIR] [--low-textures DIR] [--raw] [--scale S]
 *                          [--steer DEGREES] [--donor CAR] [--keep-pdn] [--data DIR] [--force]
 *
 * The donor is the car of the CD --donor names (a word or a stem; default the card's sound_from), read from the data root (default
 * "original"), so that importing twice gives the same file. The OBJ's MTL is its mtllib, beside it; the textures are the MTL's
 * map_Kd paths below --textures (default the MTL's directory). With --raw the OBJ is unpack's, its tables beside it. Writes the
 * piece's CFM and the body's measured half sizes into its PDN's first three words (the rest of the PDN as it was; --keep-pdn
 * leaves it alone and says so), and removes the piece's car masks, which paint the template's textures. With --raw the piece's banks
 * stay when they are a retouch of the donor's (its entries, its palettes: car resize's other sizes), and Blender's uvs are read with
 * their sizes. Without --force it
 * replaces only the CFM car new copied (the donor's). Everything is read and made before anything is written. */
#ifndef TNFS_STUDIO_CAR_IMPORT_H
#define TNFS_STUDIO_CAR_IMPORT_H

#include "formats/exchange.h"
#include "formats/mod_ini.h"

/* argv[0] is "car", argv[1] "import". 0 with `err` filled on failure. */
int car_import_command(int argc, char **argv, ExchangeError *err);
/* the car piece's mod.ini in `info` (type=car with a stem); 0 with `err` filled otherwise */
int car_piece_card(const char *piece, ModInfo *info, ExchangeError *err);

#endif
