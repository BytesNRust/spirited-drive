/* port (sky_sun_disc): the sun in the sky, a soft disc and its halo at the time's `sun AZIMUTH ELEVATION` (the direction the shadows are cast from), drawn over the sky and before the 3D, so the mountains and the road cover it.
 * Restomod only, where sun_shadow is on; never in fog, rain, snow or at night (the sun is not there). The geometry is plain maths on the view and the camera matrix; the drawing reuses the lamps' dithered halo
 * (lamp_flare.h) with tables made from the frame's own palette. */
#ifndef TNFS_SKY_SUN_H
#define TNFS_SKY_SUN_H

#include "raster/view.h"

#include <stdint.h>

enum {
    SKY_SUN_CORE_TENTHS = 22,   /* the disc's radius, tenths of a degree (the real sun is half a degree: this is the sun as it reads in a picture) */
    SKY_SUN_HALO_TENTHS = 120   /* the halo's radius, tenths of a degree */
};

typedef struct {
    int x, y;       /* the disc's centre on the frame, in pixels */
    int core, halo; /* the radii, pixels */
} SkySunDisc;

/* Where the sun is on the frame: `azimuth` in radians from the world's +x toward +z, `elevation` over the horizon, as TerrainLightSun has them (the world's y is up); `matrix` the camera's (16.16, as the sky
 * ring uses it); the camera looks along +z of its own space, +x to its right. 1 and `out` filled when the sun is in front of the camera (its centre may lie outside the frame: the halo can still reach it),
 * else 0. */
int sky_sun_place(const View *view, const int32_t matrix[9], float azimuth, float elevation, SkySunDisc *out);

/* Draws the halo and the disc at `disc` into `pixels` (8-bit, `pitch` wide) inside x0..x1 x y0..y1 (inclusive): each pixel takes the palette's entry nearest to its own colour plus the sun's, three dither levels. */
void sky_sun_draw(const SkySunDisc *disc, const uint8_t palette[768], uint8_t *pixels, int pitch, int x0, int y0, int x1, int y1);

#endif
