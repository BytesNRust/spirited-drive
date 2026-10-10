/* Development rasteriser for the track viewer: perspective-correct textured
 * convex polygons into an 8-bit buffer, clipped to the near plane and the
 * screen. Not the original renderer (0044b510 / 00417404, not ported yet). */
#ifndef TNFS_RASTER_H
#define TNFS_RASTER_H

#include "formats/fsh.h"

#include <stdint.h>

typedef struct {
    float x, y, z;  /* camera space: x right, y up, z forward */
    float u, v;     /* texels */
} RasterVertex;

typedef struct {
    uint8_t *pixels;
    int width, height;
    float focal;    /* pixels per unit at depth 1 */
    float near_z;
} RasterTarget;

/* count <= 8. texture NULL draws flat colour. transparent skips texel 0xFF. */
void raster_polygon(const RasterTarget *t, const RasterVertex *in, int count, const FshEntry *texture,
                    int transparent, uint8_t colour);

/* Screen-space quad with affine texture mapping: screen and uv are 4 corners
 * of (x, y) pixels and (u, v) 16.16. A stand-in for EA's quad mapper until
 * the port of 00487180 lands. */
void raster_screen_quad(const RasterTarget *t, const int32_t screen[8], const int32_t uv[8], const FshEntry *texture,
                        int transparent);

/* Screen-space triangle with affine texture mapping: screen is 3 corners of
 * (x, y) pixels, uv 3 corners of (u, v) texels (not 16.16). */
void raster_screen_triangle(const RasterTarget *t, const int32_t screen[6], const int32_t uv[6], const FshEntry *texture,
                            int transparent);

/* A solid rectangle clipped to the target. */
void raster_fill(const RasterTarget *t, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t colour);

#endif
