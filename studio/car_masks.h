/* tnfs-studio car masks: the colour masks a kit's author paints made into the game's grey ones (dep59 car editor, 1b).
 *
 *   tnfs-studio car masks KIT_DIR [--stem STEM] [--force]
 *
 * Each KIT_DIR/mascaras/NAME.png (NAME: 4 letters or digits, then _lo for the far model's bank; a picture in the colours of the mask layer, the ones the
 * PSD tool reads: tools/car_mask_from_psd.py) is classed texel by texel (car_mask_class_rgba) and written as KIT_DIR/coche/car_masks/STEM/NAME_mask.png,
 * a grey 8-bit PNG of the same size, the file the game and the editor read. STEM is the one of KIT_DIR/coche/mod.ini unless --stem names it. A mask in
 * that folder that has no picture is left as it is. Nothing is overwritten without --force. The editor's Build calls car_masks_make too. */
#ifndef TNFS_STUDIO_CAR_MASKS_H
#define TNFS_STUDIO_CAR_MASKS_H

#include "formats/exchange.h"

#include <stdio.h>

/* Makes the masks of the kit `kit` (see above); `stem` NULL: the card's. One line a mask is said on `log` (may be NULL). The number of masks written, or -1 with `err` filled
 * (every picture is read and classed before anything is written). */
int car_masks_make(const char *kit, const char *stem, int force, FILE *log, ExchangeError *err);

/* argv[0] is "car", argv[1] "masks". 0 with `err` filled on failure. */
int car_masks_command(int argc, char **argv, ExchangeError *err);

#endif
