/* port: the PS1 look's light script (re/notes/psx-light.md). Z<TRACK><A|B|C>.LGS is text: C comments, then the depth cue's
 * four points (X,Y percent), the default ambient colour at the start and at the end (128 is neutral, per channel) and the
 * light contexts. Only the ambient colour is read here; the contexts (the cars' light by segment range) are not. Numbers are
 * separated by anything that is not a digit, so commas, spaces and CR / LF all do, and the file's trailing binary after the
 * last line ("CRCF" and padding) ends the read. */
#ifndef TNFS_PSX_LGS_H
#define TNFS_PSX_LGS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int depth[8];       /* X0,Y0 .. X3,Y3 */
    int start[3];       /* the ambient colour at the track's start: r, g, b */
    int end[3];         /* and at its end */
} PsxLgs;

/* 1 and `out` set when the text has the depth cue and both ambient colours (each channel 0..255); 0 otherwise. */
int psx_lgs_parse(const uint8_t *file, size_t size, PsxLgs *out);

/* The ambient colour as a multiplier of each channel, 1.0 for 128; the mean of the start and end colours. */
void psx_lgs_tint(const PsxLgs *lgs, float tint[3]);

/* `out` (768 bytes, 8 bits a channel) is `palette` with every entry multiplied by `tint` (per channel, clamped to 255,
 * rounded). A tint of the palette keeps each index its own colour, so a texture's variation survives, where moving each
 * polygon's colours to the nearest palette entry gathers them into the few dark ones the palette has. It is used for the 3D's
 * pixels only (psx_world_mask): the HUD and the dash index the same entries. */
void psx_tint_palette(const uint8_t palette[768], const float tint[3], uint8_t out[768]);

/* Which pixels of a finished frame are the 3D world's: `world` is the frame as it was when the 3D was done and the 2D not yet
 * drawn, `shown` the frame now; a pixel the 2D layer changed is not the world's (mask 0), the others are (1). */
void psx_world_mask(const uint8_t *shown, const uint8_t *world, size_t count, uint8_t *mask);

#endif
