/* tnfs-studio car kit and car build: a car's flat sources and the car made from them (dep85; formats/car_kit.h, formats/car_kit_paths.h).
 *
 *   tnfs-studio car kit PIECE_DIR KIT_DIR
 *   tnfs-studio car build KIT_DIR
 *
 * car kit lays a car piece (its mod.ini, card, SIMDATA files, masks and photos) out as a kit's sources, with a README.txt, into a folder
 * that has no files yet: the masks lose their stem level, the photos go to photos/. car build makes build/piece/ (the piece as the game
 * installs it) and build/<id>.zip from the kit's sources alone, and writes nothing else: build/ is the build's, made again each time. */
#ifndef TNFS_STUDIO_CAR_KIT_H
#define TNFS_STUDIO_CAR_KIT_H

#include "formats/exchange.h"

/* car build, without the talk: build/piece/ and build/<id>.zip made again from the kit's sources (the Studio window's Build runs it in a thread). `verbose`
 * prints what car build prints. 0 with `err` filled on failure. */
int car_kit_build_dir(const char *kit, int verbose, ExchangeError *err);

/* the kit `kit` (an empty or new folder) laid out from the car piece `piece`, as car kit does it, without the talk */
int car_kit_from_piece(const char *piece, const char *kit, ExchangeError *err);

/* the CFM `kit`'s sources make, built the way car build does, without the piece around it (no card, PBS or PDN needed, nothing written): in *cfm, malloc'd. For
 * the migration of the originals (a traffic car has no PBS, so car build refuses it), which checks the CFM is the original's byte for byte. */
int car_kit_cfm_of_dir(const char *kit, uint8_t **cfm, size_t *cfm_len, ExchangeError *err);

/* argv[0] is "car", argv[1] "kit" or "build". 0 with `err` filled on failure. */
int car_kit_command(int argc, char **argv, ExchangeError *err);
int car_build_command(int argc, char **argv, ExchangeError *err);

#endif
