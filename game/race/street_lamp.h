/* port (night, psx_headlights): the street lamps of City's first track. Its lamp posts are one scenery board (a 64 x 128 sprite of a post with its arm; the
 * descriptor's resource 40, scenery texture 10; 174 of them along CY1) whose lens is a strip of amber texels at the top right. At night the lens is
 * drawn in an entry of its own the tint leaves alone, and a flare (a warm halo of dithered texels under the lens, on the sprite's transparent texels) is
 * painted round it. The entries are the lamp pool's (drive_light.c); no PS1 data is used. */
#ifndef TNFS_STREET_LAMP_H
#define TNFS_STREET_LAMP_H

#include <stddef.h>
#include <stdint.h>

enum { STREET_LAMP_ENTRIES = 3 };   /* palette entries it needs: the lens, the halo's core and its outer rim */

typedef struct {
    int scenery;                          /* the board: TrackArt.scenery[scenery] */
    int width, height;                    /* its size, to be sure it is the one */
    int lens_rect[4];                     /* x0, y0, x1, y1 inclusive: every opaque texel in it is the lens (the board's colours are its own palette's, matched to the race's) */
    int halo_x, halo_y;                   /* the halo's centre */
    int core, rim;                        /* its radii, in texels */
    uint8_t transparent;                  /* the board's transparent entry */
} StreetLamp;

/* The track's street lamp (CY1, any case): 1 and `*out` set, 0 for the others. */
int street_lamp_for(const char *track, StreetLamp *out);

typedef struct {
    size_t lens;   /* texels turned to the lens entry */
    size_t halo;   /* transparent texels turned to a halo entry */
} StreetLampPaint;

/* Paints the board's `pixels` (width x height palette indices) for the night: the lens texels in entry[0], the transparent texels within the core in entry[1]
 * (a checker, half of them) and within the rim in entry[2] (every fourth). Nothing else changes; painting again changes nothing. Zero counts when the
 * board is not the lamp's size. */
StreetLampPaint street_lamp_paint(const StreetLamp *lamp, uint8_t *pixels, int width, int height, const uint8_t entry[STREET_LAMP_ENTRIES]);

#endif
