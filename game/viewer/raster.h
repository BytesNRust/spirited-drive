/* Development rasteriser for the track viewer: perspective-correct textured
 * convex polygons into an 8-bit buffer, clipped to the near plane and the
 * screen. Not the original renderer: `--track`'s stand-in view only (Space, TNFS_STAND_IN); the race draws
 * through track_render (0044b510) and the 3D engine, and ported_view takes only raster_fill from here. */
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

/* A solid rectangle clipped to the target. */
void raster_fill(const RasterTarget *t, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t colour);

#endif
