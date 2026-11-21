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

#include "formats/car_kit_check.h"
#include "formats/exchange.h"

/* car build, without the talk: build/piece/ and build/<id>.zip made again from the kit's sources (the Studio window's Build runs it in a thread). `verbose`
 * prints what car build prints. 0 with `err` filled on failure. */
int car_kit_build_dir(const char *kit, int verbose, ExchangeError *err);

/* argv[0] is "car", argv[1] "kit" or "build". 0 with `err` filled on failure. */
int car_kit_command(int argc, char **argv, ExchangeError *err);
int car_build_command(int argc, char **argv, ExchangeError *err);
/* tnfs-studio car check KIT_DIR: the kit's problems, one line each ("error|warning ID: text"), then the counts; fails when there is an error.
 * car_kit_check_dir is the same check for a caller that shows them itself (the Studio's Problems panel): 0 with `err` when the kit cannot be read. */
int car_check_command(int argc, char **argv, ExchangeError *err);
int car_kit_check_dir(const char *kit, KitProblems *out, ExchangeError *err);

#endif
