/* tnfs-studio mod pack: the zip a mod is installed from (dep59 tools, stage A).
 *
 *   tnfs-studio mod pack DIR OUT.zip [--force]
 *
 * DIR is a mod's folder with its mod.ini. It must pass validate (a car piece its check, studio/mod_validate.h); the system's litter
 * (.DS_Store and the like) is left out and any other file the installation does not take refuses the pack, named. The zip is
 * stored, mod.ini first and the rest by name, so the same folder gives the same bytes; it must pass the installation's own check
 * before it is written, at once, and an existing OUT.zip is kept without --force. */
#ifndef TNFS_STUDIO_MOD_PACK_H
#define TNFS_STUDIO_MOD_PACK_H

#include "formats/exchange.h"

/* argv[0] is "mod". 0 with `err` filled on failure. */
int mod_command(int argc, char **argv, ExchangeError *err);

#endif
