/* port (headlight_cone): a lit road polygon the cone crosses is drawn as a grid of smaller cells, each corner lit by the cone at its own place on the ground, so the light is not a
 * straight ramp across a polygon the mesh made big (the terrain's shoulders, the far rows). Modern 3D's cells only. Off unless TNFS_CELL_ON is set: they cost a frame 3..8 % at night. */
#ifndef TNFS_CONE_CELLS_H
#define TNFS_CONE_CELLS_H

#include "game/race/psx_light.h"
#include "game/viewer/modern_road.h"

#include <stdint.h>

enum { CONE_CELLS_MAX = 4,      /* the grid's largest side */
       CONE_CELLS_PIXELS = 48   /* a cell is about this many pixels across on the screen, at least: a polygon small on the screen is not cut */ };

/* The grid of a polygon: (n + 1) x (n + 1) vertices, row j (texture's v) and column i (u), each with its camera-space place, its uv (16.16) and the darkness it is lit at. Vertex (0,0),
 * (0,n), (n,n), (n,0) are the polygon's own four corners, in cam's order. Cells share their vertices, so they meet. */
typedef struct {
    int n;
    int32_t cam[CONE_CELLS_MAX + 1][CONE_CELLS_MAX + 1][3];
    int32_t uv[CONE_CELLS_MAX + 1][CONE_CELLS_MAX + 1][2];
    float dark[CONE_CELLS_MAX + 1][CONE_CELLS_MAX + 1];
} ConeGrid;

/* Plans the grid for the polygon (uv 16.16 per cam corner; corner[4]: the corners' intensities; xz[4]: their ground places in metres; node[4]: their nodes, PSX_LIGHT_NO_NODE where unknown; the
 * texture's size picks where in the polygon each uv is). Returns the cells a side (2..CONE_CELLS_MAX), the grid in `g`; 0, `g` untouched, when the polygon is to be drawn as it was: no cone,
 * the beam does not touch it or covers it whole, the light is near the same all over it, or it is small on the screen. */
int cone_cells_grid(ConeGrid *g, const View *view, const FshEntry *texture, const int32_t uv[8], const int32_t *const cam[4], const PsxLight *light, const uint8_t corner[4],
                    const double xz[4][2], const uint32_t node[4]);

/* Draws the polygon as its grid (returns 1), or returns 0, nothing drawn, when cone_cells_grid returns 0. */
int cone_cells_draw(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8], const int32_t *const cam[4], int transparent,
                    const uint8_t (*tables)[256], const PsxLight *light, const uint8_t corner[4], const double xz[4][2], const uint32_t node[4]);

#endif
