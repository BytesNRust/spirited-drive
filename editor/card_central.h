/* «As on the card»: a car's card.png as Control Central draws it. The game (added_card_glue.c, added_card.c) dithers the picture, Floyd-Steinberg, into the palette
 * of the screen, every entry but the pointer's (0xfe) and the see-through one (0xff); this is the same code (core/raster/picture_palette.c, which the game's menus share) on the same palette. */
#ifndef TNFS_TRACK_EDITOR_CARD_CENTRAL_H
#define TNFS_TRACK_EDITOR_CARD_CENTRAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Control Central's palette, 256 colours of 8 bits a channel (768 bytes): the "!pal" of <cd>/FRONTEND/ART/CONTROL/CENTRAL.QFS, 8 bits as it is when its type is 0x24 and
 * shifted left twice (6 to 8 bits) otherwise (palette_put). 1, or 0 with `why` (English, for the person). */
int card_central_palette(const char *cd, uint8_t palette[768], char *why, size_t why_cap);

/* `rgba` (w x h) as Central draws it, into `out` (w x h RGBA, opaque) */
void card_central_as_drawn(const uint8_t *rgba, int w, int h, const uint8_t palette[768], uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif
