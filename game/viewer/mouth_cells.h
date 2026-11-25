/* port (tunnel gradient): a polygon along a tunnel's mouth, whose nodes carry the gradient's steps, drawn as one piece a node long, each piece graded by the road's light at its own nodes and seen through
 * its own step's tables. The road mesh's far polygons are 2, 4 or more nodes long: the light at their corners skips the nodes between, which are where the gradient is. */
#ifndef TNFS_MOUTH_CELLS_H
#define TNFS_MOUTH_CELLS_H

#include "game/viewer/modern_road.h"

#include <stdint.h>

/* Draws the polygon as its pieces (returns 1) when it is a strip along the road with a node of a gradient step (look 2 or 3) among its nodes and its corners' light is the light's rows' (so a night's pools
 * and the like keep the polygon whole); 0 and nothing drawn when it is to be drawn as before. `corner` the corners' light, `texture` the polygon's. */
int mouth_cells_draw(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8], const int32_t *const cam[4], int transparent, const uint8_t corner[4]);

#endif
