/* tnfs-studio fam: a stage's art file made from pictures (formats/fam_build.h), the redistributable way to dress a track with
 * textures that are not the game's.
 *
 *   tnfs-studio fam build SPEC --out FILE.FAM [--force]
 *
 * SPEC is the text of formats/fam_spec.h; its picture paths are relative to SPEC's folder. Every picture is a PNG (8-bit, indexed or
 * not). Nothing is overwritten without --force. */
#ifndef TNFS_STUDIO_FAM_COMMAND_H
#define TNFS_STUDIO_FAM_COMMAND_H

#include "formats/exchange.h"

/* argv[0] is "fam". 0 with `err` filled on failure. */
int fam_command(int argc, char **argv, ExchangeError *err);

#endif
