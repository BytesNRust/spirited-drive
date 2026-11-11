#include "game/viewer/tunnel_cells.h"

#include "game/race/track_node_light.h"

#include <math.h>

static double lerp(double a, double b, double t) { return a + (b - a) * t; }

void tunnel_cells_piece(TunnelPiece *out, int along, int pieces, int across, int j, int i, const int32_t uv[8], const int32_t *const cam[4], const double xz[4][2], const float dark[4])
{
    /* the long sides: P and Q, each from a corner to the corner it runs to; a cell's corner k is at s0 or s1 along them (S) and at t0 or t1 across, P to Q (T) */
    static const int ENDS[2][2][2] = { { { 0, 1 }, { 3, 2 } }, { { 0, 3 }, { 1, 2 } } };   /* [along][P or Q][from, to] */
    static const int S[2][4] = { { 0, 1, 1, 0 }, { 0, 0, 1, 1 } };
    static const int T[2][4] = { { 0, 0, 1, 1 }, { 0, 1, 1, 0 } };
    const int a = along != 0;
    for (int k = 0; k < 4; k++) {
        const double s = (double)(j + S[a][k]) / pieces, t = (double)(i + T[a][k]) / across;
        const int pf = ENDS[a][0][0], pt = ENDS[a][0][1], qf = ENDS[a][1][0], qt = ENDS[a][1][1];
        for (int e = 0; e < 3; e++) out->cam[k][e] = (int32_t)lrint(lerp(lerp(cam[pf][e], cam[pt][e], s), lerp(cam[qf][e], cam[qt][e], s), t));
        for (int e = 0; e < 2; e++) out->uv[2 * k + e] = (int32_t)lrint(lerp(lerp(uv[2 * pf + e], uv[2 * pt + e], s), lerp(uv[2 * qf + e], uv[2 * qt + e], s), t));
        for (int e = 0; e < 2; e++) out->xz[k][e] = lerp(lerp(xz[pf][e], xz[pt][e], s), lerp(xz[qf][e], xz[qt][e], s), t);
        out->dark[k] = (float)lerp(lerp(dark[pf], dark[pt], s), lerp(dark[qf], dark[qt], s), t);
        out->s[k] = s;
    }
}

int tunnel_cells_plan(const uint32_t node[4], int *along)
{
    const uint32_t none = TRACK_NODE_LIGHT_NO_NODE;
    if (node[0] == none || node[1] == none || node[2] == none || node[3] == none) return 0;
    uint32_t far_a, far_b;
    if (node[0] == node[3] && node[1] == node[2]) *along = 0, far_a = node[0], far_b = node[1];
    else if (node[0] == node[1] && node[3] == node[2]) *along = 1, far_a = node[0], far_b = node[3];
    else return 0;
    const uint32_t span = far_a > far_b ? far_a - far_b : far_b - far_a;
    if (span == 0 || span > 64) return 0;   /* a ring's seam, or not a strip along the road */
    return span > TUNNEL_CELLS_MAX ? TUNNEL_CELLS_MAX : (int)span;
}

