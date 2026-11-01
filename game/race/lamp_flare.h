/* port (port.cfg psx_flares, the PS1 look): a soft red halo round a car's lit brake lights. The lit brake light is a palette entry of its own (the emissive
 * lamps', lamp_emissive.h, not fogged or tinted), so the lamp's visible pixels are the pixels of that index: found in the car's screen rectangle, they are exactly what the car's
 * body and what stands in front of it leave visible, and need no model data. Each blob of them gets a dithered halo in two strengths
 * (the lamp's colour at a fifth and at under half), added to each pixel's colour and put back into the palette, so the glow stays in it. */
#ifndef TNFS_LAMP_FLARE_H
#define TNFS_LAMP_FLARE_H

#include <stdint.h>

enum {
    LAMP_FLARE_BLOBS = 8,         /* the most blobs a car gives */
    LAMP_FLARE_MAX_RADIUS = 48,   /* the halo's reach in pixels, at any render scale */
    LAMP_FLARE_LEVELS = 2         /* the halo's dither levels above none */
};

typedef struct {
    int cx, cy;       /* the pixels' centre */
    int area;         /* pixels */
    uint8_t value;    /* the pixel value the blob was found from (a lamp in fog has several: the fog's levels' images of its entry) */
} LampBlob;

/* The 4-connected blobs of pixels whose value is flagged in `member` (256 flags) in the rectangle x0..x1 x y0..y1 (inclusive, clipped by the caller to the surface), at most
 * `max`, largest first; returns how many. */
int lamp_blobs(const uint8_t *pixels, int pitch, int x0, int y0, int x1, int y1, const uint8_t member[256], LampBlob *out, int max);

/* The halo's radius for a blob of `area` pixels: the blob's own radius times 2.2, at least 3, at most LAMP_FLARE_MAX_RADIUS. */
int lamp_flare_radius(int area);

/* The halo's dither level (0 none .. LAMP_FLARE_LEVELS) at (dx, dy) from the centre: the intensity falls off with the square of the distance
 * from 1 at the centre to 0 at `radius`, and a 4 x 4 ordered dither picks the level. */
int lamp_halo_level(int dx, int dy, int radius);

/* Adds the halo at (cx, cy): each pixel of level l (1..LAMP_FLARE_LEVELS) becomes map[l - 1][pixel] (a 256-entry table of the frame's own palette:
 * the entry nearest the pixel's colour plus the level's); only inside x0..x1 x y0..y1. */
void lamp_halo_draw(uint8_t *pixels, int pitch, int x0, int y0, int x1, int y1, int cx, int cy, int radius,
                    const uint8_t *const map[LAMP_FLARE_LEVELS]);

#endif
