/* port (gauge_light): the hand-made mask of a dash's 2D layer (640 x 480, the layer's own frame): which pixels are a lit gauge, in <root>/<CAR>/dash_mask.png
 * (data/dash_masks in the source tree). Four values, by the colour painted in the PSD (tools/dash_mask_from_psd.py) or the grey the tool writes:
 *   black   0   AUTO  not marked: the cockpit's own gauge ellipses decide, as without a mask
 *   blue   64   DASH  the dash's art: never lit (the speedo's rim, the dash round a dial)
 *   green 128   DIAL  inside a dial: its bright marks (numerals, ticks) are lit, its dark face is not
 *   white 255   LIT   lit whatever its brightness (a warning lamp, a lit window)
 * A pixel the needle's colour draws is never lit. The masks hold no colour of the art. A dash with a mask is decided by it alone (the automatic
 * lit marks are off); one without keeps the automatic rules. */
#ifndef TNFS_DASH_MASK_H
#define TNFS_DASH_MASK_H

#include <stddef.h>
#include <stdint.h>

enum { DASH_MASK_AUTO = 0, DASH_MASK_DASH = 64, DASH_MASK_DIAL = 128, DASH_MASK_LIT = 255 };
enum { DASH_MASK_W = 640, DASH_MASK_H = 480 };

/* The class of a mask colour (RGB). */
int dash_mask_class(const uint8_t rgb[3]);

/* Decodes a mask PNG into `out` (width x height bytes of DASH_MASK_*), scaled from the image's own size by nearest neighbour. 1 when it reads. */
int dash_mask_parse(const uint8_t *png, size_t len, int width, int height, uint8_t *out);

/* Writes `mask` (width x height of DASH_MASK_*) as a PNG: indexed, black, blue, green, white. `*out` is freed with png_free. 0 when it cannot. */
int dash_mask_write(const uint8_t *mask, int width, int height, uint8_t **out, size_t *len);

/* The mask of the dash `stem` ("TSUPRA", as the dash's files are named; tried as given and in upper case) at DASH_MASK_W x DASH_MASK_H, from the first of
 * $TNFS_DASH_MASKS, data/dash_masks and the source tree's that has <stem>/dash_mask.png. malloc'd; NULL when there is none or it does not read. */
uint8_t *dash_mask_load(const char *stem);

/* The bytes of <root>/<stem>/<name> (stem as given), from the first of $TNFS_DASH_MASKS, data/dash_masks and the source tree's that has it: the dash's hand-made files
 * (dash_mask.png, dash_color.png). Freed with platform_free_file; NULL when there is none. */
uint8_t *dash_asset_read(const char *stem, const char *name, size_t *size);

#endif
