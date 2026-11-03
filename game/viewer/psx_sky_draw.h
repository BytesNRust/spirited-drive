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
    FshEntry in_palette;     /* `horz` with its texels already the race palette's nearest colours (the mirror's flat draw has no smooth horizon) */
    uint8_t *mapped;         /* its pixels (owned) */
    uint8_t mapped_map[256]; /* the smooth map they were made with */
    int mapped_ok;
    int star_rows, star_land_columns;   /* the last stars' band (rows) and the columns it skipped as land, for the log */
} PsxSkyDraw;

/* 1 when the sky's bitmap is made (the draw owns its memory), 0 and nothing kept otherwise. */
int psx_sky_draw_make(PsxSkyDraw *d, const PsxSky *sky, const PsxSkyInf *inf);
/* port (the PC's alternative sky): the sky of a PC horizon picture `horizon` ('horz', its texels indices of `palette`, 768 bytes), as it is, no zoom
 * and no gradient: its horizon row is the picture's misc[1]. 1 when made (copied), 0 and nothing kept otherwise. */
int psx_sky_draw_make_indexed(PsxSkyDraw *d, const FshEntry *horizon, const uint8_t palette[768]);
/* port (night): the sky's own palette multiplied per channel by `mul` (rounded, kept in 0..255), before psx_sky_draw_target matches it to the shown
 * palette; nothing when no sky is made. The night's light over a dusk sky. */
void psx_sky_draw_scale(PsxSkyDraw *d, const float mul[3]);
/* port (night, psx_headlights): a dithered layer of stars in the dark of the sky. Three of the sky's palette entries (the least used; their texels go to the nearest
 * colour left) become dim, middle and bright greys and a sprinkle of single texels takes them, thick at the top and thinning to the horizon, only where the sky
 * is dark (not over the clouds or the glow), from `seed`. Call after psx_sky_draw_scale, before psx_sky_draw_target. The number of stars; 0 when no sky is made. */
int psx_sky_draw_stars(PsxSkyDraw *d, uint32_t seed);
/* The sky as a horizon picture in the race palette (the nearest colours of the last psx_sky_draw_target's target), for the draws that have no
 * smooth horizon (the rear mirror's); NULL before the first target. Valid until the next call. */
const FshEntry *psx_sky_draw_in_palette(PsxSkyDraw *d);
/* The screen colours for the frame: `target` the palette the world is shown with (768 bytes). Cheap when it did not change. */
void psx_sky_draw_target(PsxSkyDraw *d, const uint8_t target[768]);
void psx_sky_draw_free(PsxSkyDraw *d);

#endif
