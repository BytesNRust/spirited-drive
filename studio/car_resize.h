/* tnfs-studio car resize: a texture of a car piece given another size, the uvs that address it following (dep71 follow-up,
 * notes/dep71-texture-resize-plan.md; formats/car_resize.h).
 *
 *   tnfs-studio car resize PIECE_DIR NAME WxH [--image FILE.png] [--low]
 *   tnfs-studio car resize PIECE_DIR NAME --image FILE.png [--low]
 *
 * NAME is the bank entry (rsid, topv, tyr1 ...: the names unpack's entries.tsv lists). The piece's CFM gets the bitmap at W x H
 * (the given picture, fitted to the bank's palette as car import fits one, or the bitmap's own pixels scaled) and every uv of its
 * faces scaled to it; the high model's bank, or with --low the far model's. The piece's skins of that texture
 * (skins/<skin>/NAME.png, NAME_lo.png with --low) are scaled with it so that they keep fitting; masks are scaled by the game. A
 * later `car import --raw` keeps the new size. Everything is made before anything is written. */
#ifndef TNFS_STUDIO_CAR_RESIZE_H
#define TNFS_STUDIO_CAR_RESIZE_H

#include "formats/exchange.h"

/* argv[0] is "car", argv[1] "resize". 0 with `err` filled on failure. */
int car_resize_command(int argc, char **argv, ExchangeError *err);

#endif
