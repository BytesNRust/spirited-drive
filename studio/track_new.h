/* tnfs-studio track new: a track described in text, built from the files of an existing one.
 *
 *   tnfs-studio track new LAYOUT --template SLOT --out DIR [--stem NAME] [--data DIR] [--force]
 *
 * LAYOUT is the text of formats/track_gen.h. The files of the track SLOT in the data root (default "original")
 * are the template: the TRI is rebuilt from the layout, the INFO and the FAMs (road textures, scenery objects)
 * are copied. DIR gets them laid out like the CD with the stem NAME (default: the slot's), ready for
 * `tnfs-studio slot replace track SLOT DIR --stem NAME --mods MODS`. Nothing is overwritten without --force. */
#ifndef TNFS_STUDIO_TRACK_NEW_H
#define TNFS_STUDIO_TRACK_NEW_H

#include "formats/exchange.h"

/* argv[0] is "track". 0 with `err` filled on failure. */
int track_command(int argc, char **argv, ExchangeError *err);

#endif
