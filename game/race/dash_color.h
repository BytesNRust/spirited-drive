/* port (gauge_light = 5, "Del coche"): the colours the owner painted for a dash's lit parts, in <root>/<CAR>/dash_color.png (data/dash_masks in the source tree), written by
 * tools/dash_mask_from_psd.py from the PSD's optional "color" layer. An RGBA image of the dash's 640 x 480 layer: the colour of a pixel is the colour of the light there (one
 * per dial if the owner wants); a transparent pixel, or a black one, has none. The lit parts (the mask's, or the automatic ones) at a pixel with a colour take it; the others
 * take gauge_light 3's orange. The colours are folded into DASH_COLOUR_SLOTS flat ones (a lit palette each, drive_light_palette_lit_colour): a pixel goes to the slot of the
 * nearest colour, and a colour near an earlier one (SLOT_NEAR) shares its slot. */
#ifndef TNFS_DASH_COLOR_H
#define TNFS_DASH_COLOR_H

#include <stddef.h>
#include <stdint.h>

enum { DASH_COLOUR_SLOTS = 12 };

typedef struct {
    uint8_t *slot;                           /* DASH_MASK_W x DASH_MASK_H: 0 no colour, else 1..count */
    int count;                               /* the colours in use */
    uint8_t rgb[DASH_COLOUR_SLOTS][3];       /* slot s (1-based) is rgb[s - 1] */
} DashColour;

/* Decodes a colour PNG into `out->slot` (width x height) by nearest neighbour from the image's own size. `out->slot` is malloc'd by the call. 1 when it reads. */
int dash_color_parse(const uint8_t *png, size_t len, int width, int height, DashColour *out);

/* The colours of the dash `stem` (as dash_mask_load looks a mask up). 1 and `out` filled (free with dash_color_free); 0 when there is none or it does not read. */
int dash_color_load(const char *stem, DashColour *out);
void dash_color_free(DashColour *c);

#endif
