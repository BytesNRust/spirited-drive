/* port (tunnel_lamps): a tunnel's polygon with lamps near it, drawn as one piece a node long along the road, each piece graded by the lamps' heat at its own corners. The road mesh's far
 * polygons are 2, 4 or more nodes long and the lamps hang every 2: the heat at a big polygon's four corners is not the pools between them. */
#ifndef TNFS_TUNNEL_CELLS_H
#define TNFS_TUNNEL_CELLS_H

#include "game/viewer/modern_road.h"

#include <stdint.h>

enum { TUNNEL_CELLS_MAX = 8, TUNNEL_CELLS_ACROSS_MAX = 4, TUNNEL_CELL_METRES = 4 };   /* the cells along a polygon at most (its length in nodes), across it at most, and about how wide one is across */

/* One cell of a polygon cut into `pieces` along the road and `across` across it: its camera-space corners (cam's order), uv and the corners' darkness, at s = j / pieces .. (j + 1) / pieces along
 * and t = i / across .. (i + 1) / across across. `along` 0: the long sides are corners 0-1 and 3-2; 1: 0-3 and 1-2. The cell's corners are in the polygon's order. */
typedef struct { int32_t cam[4][3]; int32_t uv[8]; double xz[4][2]; float dark[4]; double s[4]; } TunnelPiece;   /* s: where along the long sides each corner is, 0..1 */
void tunnel_cells_piece(TunnelPiece *out, int along, int pieces, int across, int j, int i, const int32_t uv[8], const int32_t *const cam[4], const double xz[4][2], const float dark[4]);

/* How many pieces (1..TUNNEL_CELLS_MAX) and which long sides, from the corners' nodes (0 when the polygon is not a strip of the road: no two corners share a node), 0 pieces: draw it whole. */
int tunnel_cells_plan(const uint32_t node[4], int *along);

/* Draws the polygon as its pieces (returns 1) when the track's light has tunnel lamps and the polygon is long enough to hold more than one piece or has lamps near; 0 and nothing drawn when it is to
 * be drawn as before. `tables` the tunnel's tables, `dark` the corners' darkness. */
int tunnel_cells_draw(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8], const int32_t *const cam[4], int transparent, const uint8_t (*tables)[256],
                      const float dark[4]);

#endif
