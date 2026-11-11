#include "game/viewer/tunnel_cells.h"

#include "game/race/track_node_light.h"
#include "game/race/track_fence.h"
#include "game/viewer/view_tables.h"
#include "viewer/view_shared.h"

#include <math.h>

static double lerp(double a, double b, double t) { return a + (b - a) * t; }

/* whether the light has a tunnel lamp (kept for the light it was asked of) */
static int has_tunnel_lamps(const TrackNodeLight *light)
{
    static const TrackNodeLight *seen;
    static int seen_n = -1, seen_has;
    if (light == seen && light->lamp_n == seen_n) return seen_has;
    int has = 0;
    for (int i = 0; light->lamps != NULL && i < light->lamp_n && !has; i++) has = light->lamps[i].tunnel != 0;
    seen = light, seen_n = light->lamp_n, seen_has = has;
    return has;
}

int tunnel_cells_draw(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8], const int32_t *const cam[4], int transparent, const uint8_t (*tables)[256],
                      const float dark[4])
{
    if (light_effects == NULL || light_effects->lamp_n <= 0 || !track_light_xz_ok || (track_light_tunnel & 7) != 1 || !tables_made || !has_tunnel_lamps(light_effects)) return 0;
    int along;
    const int pieces = tunnel_cells_plan(track_light_node, &along);
    if (pieces == 0) return 0;
    const uint32_t node_a = track_light_node[0], node_b = along ? track_light_node[3] : track_light_node[1];   /* the long sides run from node_a to node_b */
    const double xz[4][2] = { { track_light_xz[0][0], track_light_xz[0][1] }, { track_light_xz[1][0], track_light_xz[1][1] }, { track_light_xz[2][0], track_light_xz[2][1] },
                              { track_light_xz[3][0], track_light_xz[3][1] } };
    const uint32_t saved_node[4] = { track_light_node[0], track_light_node[1], track_light_node[2], track_light_node[3] };
    double saved_xz[4][2];
    for (int k = 0; k < 4; k++) saved_xz[k][0] = track_light_xz[k][0], saved_xz[k][1] = track_light_xz[k][1];
    const int p0 = along ? 0 : 0, q0 = along ? 1 : 3;   /* a corner of each long side: the ground between them is the polygon's width */
    const double wide = hypot(xz[p0][0] - xz[q0][0], xz[p0][1] - xz[q0][1]);
    int across = (int)lrint(wide / TUNNEL_CELL_METRES);
    across = across < 1 ? 1 : across > TUNNEL_CELLS_ACROSS_MAX ? TUNNEL_CELLS_ACROSS_MAX : across;
    for (int j = 0; j < pieces; j++)
        for (int i = 0; i < across; i++) {
            TunnelPiece piece;
            tunnel_cells_piece(&piece, along, pieces, across, j, i, uv, cam, xz, dark);
            const int32_t *const pc[4] = { piece.cam[0], piece.cam[1], piece.cam[2], piece.cam[3] };
            for (int k = 0; k < 4; k++) {   /* the cell's own places: ground and nodes (by how far along the long sides it is) */
                track_light_xz[k][0] = piece.xz[k][0], track_light_xz[k][1] = piece.xz[k][1];
                track_light_node[k] = (uint32_t)lrint(lerp((double)node_a, (double)node_b, piece.s[k]));
            }
            float heat[4];
            const uint8_t (*top)[256] = NULL;
            if (lamp_polygon_tunnel(heat, &top)) modern_road_cell_lit_warm(q, view, texture, piece.uv, pc, transparent, tables, piece.dark, top, heat, LAMP_STEPS);
            else modern_road_cell_lit(q, view, texture, piece.uv, pc, transparent, tables, piece.dark);
        }
    for (int k = 0; k < 4; k++) track_light_node[k] = saved_node[k], track_light_xz[k][0] = saved_xz[k][0], track_light_xz[k][1] = saved_xz[k][1];
    return 1;
}
