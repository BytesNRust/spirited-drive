/* The pictures a track's art is made of (fam_build.h): an RGBA picture resampled to another size, and the one 256-colour palette
 * a stage's terrain shares, quantised by median cut over 6-bit colours. The game reads a palette as 6-bit values (type 0x22, shifted
 * left by 2 by fsh_palette_rgb), so the palette is 6-bit and a colour is exact in the game. Index 0xFF is given only to a see-through pixel
 * (alpha under FAM_SEEN_ALPHA): a bitmap holding it is drawn see-through (track_art.c), as the fences' are. */
#ifndef TNFS_FAM_IMAGE_H
#define TNFS_FAM_IMAGE_H

#include <stddef.h>
#include <stdint.h>

enum {
    FAM_PALETTE_BYTES = 768,
    FAM_PALETTE_COLOURS = 256,
    FAM_PIXEL_INDICES = 255,   /* 0..254 are the pixels' indices; 0xFF is the transparent one */
    FAM_TRANSPARENT = 0xFF,
    FAM_SEEN_ALPHA = 128       /* a pixel with less alpha is see-through */
};

typedef struct {
    uint32_t width, height;
    const uint8_t *rgba;       /* width x height x 4 bytes, row by row */
} FamPicture;

/* `src` box-filtered to width x height (malloc'd, width x height x 4 bytes); NULL when memory runs out. Alpha is the box's mean; where a box
 * has see-through texels, the colour is weighted by alpha (the seen texels'), so a fence's edge does not take the colour of the gaps. */
uint8_t *fam_resample(const FamPicture *src, uint32_t width, uint32_t height);

/* The palette of `n` pictures (6-bit RGB, 768 bytes, into palette6): the median cut of their seen colours (see-through pixels left out)
 * into 255 boxes; the 256th entry is black and unused. 0 when memory runs out. */
/* the race's shadow shade (HUD_SHADE_SCALE, game/race/hud_colours.h: 0.6) and the farthest a colour's shade may be from the palette's nearest entry, squared in
 * 8-bit units: the CD's own AL1 has its yellow's shade at 450 (a dark yellow), and the palette keeps entries back to stay under it (fam_palette_make) */
enum { FAM_SHADE_SCALE = 0x9999, FAM_SHADE_BAR = 450 };

int fam_palette_make(const FamPicture *pics, size_t n, uint8_t palette6[FAM_PALETTE_BYTES]);

/* The index 0..254 of the palette entry nearest (r, g, b). */
uint8_t fam_palette_index(const uint8_t palette6[FAM_PALETTE_BYTES], uint8_t r, uint8_t g, uint8_t b);

/* The palette index of every pixel of `pic`, FAM_TRANSPARENT for a see-through one (malloc'd, width x height bytes); NULL when memory runs out. */
uint8_t *fam_indices(const FamPicture *pic, const uint8_t palette6[FAM_PALETTE_BYTES]);

#endif
