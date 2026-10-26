/* port: the PS1 sky as the race draws it (re/notes/psx-look.md, psx_sky_bitmap.h): at dawn and dusk the flat horizon is drawn
 * from the PS1's panorama and gradient instead of the track's own sky and ring, through the smooth horizon (horizon_smooth.h) so
 * the bitmap's own 256 colours reach the screen as the nearest of the race's palette as the world is shown (tinted), dithered. */
#ifndef TNFS_PSX_SKY_DRAW_H
#define TNFS_PSX_SKY_DRAW_H

#include "game/race/psx_sky.h"
#include "game/race/psx_sky_bitmap.h"
#include "formats/fsh.h"
#include "viewer/horizon_smooth.h"

typedef struct PsxSkyDraw {
    PsxSkyBitmap bitmap;
    FshEntry horz;           /* the bitmap as the flat horizon reads it (misc[1] the horizon row) */
    HorizonSmooth smooth;
    int ready;
} PsxSkyDraw;

/* 1 when the sky's bitmap is made (the draw owns its memory), 0 and nothing kept otherwise. */
int psx_sky_draw_make(PsxSkyDraw *d, const PsxSky *sky, const PsxSkyInf *inf);
/* The screen colours for the frame: `target` the palette the world is shown with (768 bytes). Cheap when it did not change. */
void psx_sky_draw_target(PsxSkyDraw *d, const uint8_t target[768]);
void psx_sky_draw_free(PsxSkyDraw *d);

#endif
