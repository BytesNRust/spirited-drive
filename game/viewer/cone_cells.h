/* port (headlight_cone): a lit road polygon the cone crosses is drawn as a grid of smaller cells, each corner lit by the cone at its own place on the ground, so the light is not a
 * straight ramp across a polygon the mesh made big (the terrain's shoulders, the far rows). Modern 3D's cells only. */
#ifndef TNFS_CONE_CELLS_H
#define TNFS_CONE_CELLS_H

#include "game/race/psx_light.h"
#include "game/viewer/modern_road.h"

#include <stdint.h>

/* Draws the cell as a grid (returns 1) when the cone is on the polygon with a corner's light unlike another's or the polygon is wide and in the beam; returns 0, nothing drawn, when
 * the cell is to be drawn as it was. uv 16.16 per cam corner as modern_road_cell_lit's; corner[4]: the polygon's corners' intensities; xz[4]: their ground places in metres, node[4]: their nodes (for a street lamp's pool), in the same
 * order (texture (0,0), (w,0), (w,h), (0,h)); the cell's uv fractions of the texture (w, h in 16.16) pick where in the polygon each of its corners is. */
int cone_cells_draw(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8], const int32_t *const cam[4], int transparent,
                    const uint8_t (*tables)[256], const PsxLight *light, const uint8_t corner[4], const double xz[4][2], const uint32_t node[4]);

/* the cells and polygons drawn so far (dev: TNFS_CELL_STATS prints them at exit) */
void cone_cells_stats(long *cells, long *polygons);

#endif
