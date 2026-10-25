/* port: the PS1 look's light script (re/notes/psx-light.md). Z<TRACK><A|B|C>.LGS is text: C comments, then the depth cue's
 * four points (X,Y percent), the default ambient colour at the start and at the end (128 is neutral, per channel) and the
 * light contexts. Only the ambient colour is read here; the contexts (the cars' light by segment range) are not. Numbers are
 * separated by anything that is not a digit, so commas, spaces and CR / LF all do, and the file's trailing binary after the
 * last line ("CRCF" and padding) ends the read. */
#ifndef TNFS_PSX_LGS_H
#define TNFS_PSX_LGS_H

#include <stddef.h>
#include <stdint.h>

enum { PSX_LGS_CONTEXTS = 32, PSX_LGS_FIXED = 64, PSX_LGS_RAMP = 96, PSX_LGS_BLEND = 2 };

typedef struct {
    int seg_ini, seg_fin;   /* the segments the context covers, both ends included */
    int mode;               /* PSX_LGS_FIXED: c1 throughout; PSX_LGS_RAMP: c1 at seg_ini to c2 at seg_fin */
    int c1[3], c2[3];
} PsxLgsContext;

typedef struct {
    int depth[8];       /* X0,Y0 .. X3,Y3 */
    int start[3];       /* the ambient colour at the track's start: r, g, b */
    int end[3];         /* and at its end */
    int context_count;  /* the contexts read (those after PSX_LGS_CONTEXTS are dropped; none when the file stops early) */
    PsxLgsContext contexts[PSX_LGS_CONTEXTS];
} PsxLgs;

/* 1 and `out` set when the text has the depth cue and both ambient colours (each channel 0..255); 0 otherwise. */
int psx_lgs_parse(const uint8_t *file, size_t size, PsxLgs *out);

/* The ambient colour as a multiplier of each channel, 1.0 for 128; the mean of the start and end colours. */
void psx_lgs_tint(const PsxLgs *lgs, float tint[3]);

/* The ambient colour the cars and the dash have at `segment` of a track of `segment_count`, as a multiplier of each channel (1.0
 * for 128): inside a context its colour (fixed, or ramped across its range), else the start colour ramped to the end across the
 * track; PSX_LGS_BLEND segments outside a context its colour fades in and out into the default one. */
void psx_lgs_ambient(const PsxLgs *lgs, int segment, int segment_count, float out[3]);

/* 1 when `segment` is inside a PSX_LGS_RAMP context (kUseAC + kSwitchAC, 96): the tunnels, where the ceiling lamps switch the cars' light;
 * the edges are exact, no blend. 0 for NULL. */
int psx_lgs_switched(const PsxLgs *lgs, int segment);

/* The ambient at `segment` over the world's `tint`, per channel, rounded to 1/`step` and at most 2 (a tint under 0.01 counts as 0.01): what the
 * cars' and the dash's colours are scaled by. Exactly 1 where the ambient is what the world already has. */
void psx_lgs_ratio(const PsxLgs *lgs, int segment, int segment_count, const float tint[3], int step, float out[3]);

/* A car's colour table under an ambient colour that differs from the world's: index c maps to the palette entry nearest the colour
 * of c times `ratio` (per channel, ambient / the world's tint); the shadow and see-through indices (0xfe, 0xff) stay. Ratios of 1 give the
 * identity. */
void psx_lit_table(const uint8_t palette[768], const float ratio[3], uint8_t out[256]);

/* `out` (768 bytes, 8 bits a channel) is `palette` with every entry multiplied by `tint` (per channel, clamped to 255,
 * rounded). A tint of the palette keeps each index its own colour, so a texture's variation survives, where moving each
 * polygon's colours to the nearest palette entry gathers them into the few dark ones the palette has. It is used for the 3D's
 * pixels only (psx_world_mask): the HUD and the dash index the same entries. */
void psx_tint_palette(const uint8_t palette[768], const float tint[3], uint8_t out[768]);

/* Which pixels of a finished frame are the 3D world's: `world` is the frame as it was when the 3D was done and the 2D not yet
 * drawn, `shown` the frame now; a pixel the 2D layer changed is not the world's (mask 0), the others are (1). */
void psx_world_mask(const uint8_t *shown, const uint8_t *world, size_t count, uint8_t *mask);

/* The rectangle (x, y, w, h) of a mask `width` x `height` set to 1, clipped to it: the rear-view mirror's picture, which the 2D layer
 * carries but shows the 3D. */
void psx_mask_rect(uint8_t *mask, int width, int height, int x, int y, int w, int h);

#endif
