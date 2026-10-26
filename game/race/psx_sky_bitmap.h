/* port: the PS1 sky (psx_sky.h) as the bitmap the flat horizon draws (horizon.h: a "horz" FshEntry of 8-bit indices, the first
 * pixel's colour filling above it and the last's below). The panorama is magnified PSX_SKY_ZOOM times each way (the PC's texel is
 * half the panorama's, at the flat horizon's turn of 1280 texels), the INF's gradient is PSX_SKY_GRADIENT_ROWS rows above it, and
 * the whole is cut to 256 colours (median cut), so the smooth horizon's blend (horizon_smooth.h) can draw it: the palette is its
 * own, not the race's. The panorama's row at PSX_SKY_HORIZON_NUM / DEN of its height sits on the horizon line. */
#ifndef TNFS_PSX_SKY_BITMAP_H
#define TNFS_PSX_SKY_BITMAP_H

#include "game/race/psx_sky.h"

#include <stdint.h>

/* the panorama's horizon row, as a part of its height: the skyline's foot, where the ground below it starts (ZTR1's 144 rows: ~88) */
enum { PSX_SKY_ZOOM = 2, PSX_SKY_GRADIENT_ROWS = 160, PSX_SKY_HORIZON_NUM = 11, PSX_SKY_HORIZON_DEN = 18,
       /* the panorama is turned by this part of its width: the PS1's ring has a seam (ZTR1A's sun is cut by it) that is kept behind the
        * camera at a race's start */
       PSX_SKY_TURN_NUM = 1, PSX_SKY_TURN_DEN = 2 };

typedef struct {
    int width, height;
    int horizon_row;       /* the row that sits on the horizon line */
    uint8_t *pixels;       /* width x height palette indices, owned */
    uint8_t palette[768];  /* 8 bits a channel */
} PsxSkyBitmap;

/* 0 (pixels NULL) for an empty panorama or on no memory. */
int psx_sky_bitmap_build(const PsxSky *sky, const PsxSkyInf *inf, PsxSkyBitmap *out);
void psx_sky_bitmap_free(PsxSkyBitmap *bitmap);

#endif
