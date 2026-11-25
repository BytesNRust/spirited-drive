#include "game/viewer/mouth_cells.h"

#include "game/race/track_fence.h"
#include "game/race/track_node_light.h"
#include "game/viewer/tunnel_cells.h"
#include "game/viewer/view_tables.h"
#include "viewer/view_shared.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* the point of a row (0..10) whose light is a at node na and b at node nb: the strip the polygon's long side runs along; -1 when none */
static int strip_point(const TrackNodeLight *light, uint32_t na, uint32_t nb, uint8_t a, uint8_t b)
{
    for (int p = 0; p < TRACK_NODE_LIGHT_POINTS; p++)
        if (track_node_light_at(light, na, p) == a && track_node_light_at(light, nb, p) == b) return p;
    return -1;
}

/* the mean brightness (0..255) of a texture through the palette, kept for the last few textures asked */
static float texture_lum(const FshEntry *t)
{
    static const FshEntry *seen[8];
    static float lum[8];
    static int next;
    for (int i = 0; i < 8; i++)
        if (seen[i] == t) return lum[i];
    double sum = 0;
    size_t n = 0;
    for (size_t i = 0; t != NULL && t->payload != NULL && i < (size_t)t->width * (size_t)t->height; i++, n++) {
        const uint8_t *const c = tone_palette + 3 * (size_t)t->payload[i];
        sum += 0.30 * c[0] + 0.59 * c[1] + 0.11 * c[2];
    }
    seen[next] = t, lum[next] = n > 0 ? (float)(sum / (double)n) : 255.0f, next = (next + 1) % 8;
    return lum[(next + 7) % 8];
}

/* the gradient step (tone id) a node carries, 0 none */
static int step_id(const TrackNodeLight *light, uint32_t node)
{
    if (light->tone_id == NULL || light->nodes == 0) return 0;
    node = track_node_light_old_node(light, node) % light->nodes;   /* the slots are in the file's own direction */
    const int id = light->tone_id[node];
    return id > 0 && id <= light->tone_count && (light->tone[id - 1].look == 2 || light->tone[id - 1].look == 3) ? id : 0;
}

int mouth_cells_draw(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8], const int32_t *const cam[4], int transparent, const uint8_t corner[4])
{
    const char *const off = getenv("TNFS_GRADIENT_OUTER");   /* dev: 0 the mouth's polygons are drawn whole, as before the gradient's outer steps */
    if (off != NULL && off[0] == '0' && off[1] == '\0') return 0;
    const TrackNodeLight *const light = light_effects;
    if (light == NULL || light->tone_id == NULL || light->tone_count <= 0 || !track_light_xz_ok || !tables_made) return 0;
    int along;
    const int pieces = tunnel_cells_plan(track_light_node, &along);
    if (pieces < 1) return 0;
    const uint32_t node_a = track_light_node[0], node_b = along ? track_light_node[3] : track_light_node[1];
    const uint32_t lo = node_a < node_b ? node_a : node_b, hi = node_a < node_b ? node_b : node_a;
    if (hi - lo != (uint32_t)pieces) return 0;
    int steps = 0;
    for (uint32_t n = lo; n <= hi; n++) steps += step_id(light, n) != 0;
    if (steps == 0) return 0;
    /* the long sides P and Q (tunnel_cells_piece's ends): their corners' rows */
    static const int ENDS[2][2][2] = { { { 0, 1 }, { 3, 2 } }, { { 0, 3 }, { 1, 2 } } };
    const int pp = strip_point(light, node_a, node_b, corner[ENDS[along][0][0]], corner[ENDS[along][0][1]]);
    const int pq = strip_point(light, node_a, node_b, corner[ENDS[along][1][0]], corner[ENDS[along][1][1]]);
    if (pp < 0 || pq < 0) return 0;
    static const int S[2][4] = { { 0, 1, 1, 0 }, { 0, 0, 1, 1 } };
    static const int T[2][4] = { { 0, 0, 1, 1 }, { 0, 1, 1, 0 } };
    const double xz[4][2] = { { track_light_xz[0][0], track_light_xz[0][1] }, { track_light_xz[1][0], track_light_xz[1][1] }, { track_light_xz[2][0], track_light_xz[2][1] },
                              { track_light_xz[3][0], track_light_xz[3][1] } };
    const int saved_tunnel = track_light_tunnel;
    const int base_all = saved_tunnel & 7;
    const float none[4] = { 0, 0, 0, 0 };
    for (int j = 0; j < pieces; j++) {
        TunnelPiece piece;
        tunnel_cells_piece(&piece, along, pieces, 1, j, 0, uv, cam, xz, none);
        const uint32_t from = (uint32_t)lrint((double)node_a + ((double)node_b - (double)node_a) * (double)j / pieces);
        const uint32_t to = (uint32_t)lrint((double)node_a + ((double)node_b - (double)node_a) * (double)(j + 1) / pieces);
        const int in_tunnel = track_node_light_tunnel(light, from) && track_node_light_tunnel(light, to);
        for (int k = 0; k < 4; k++) {
            const uint32_t at = S[along][k] ? to : from;
            const int point = T[along][k] ? pq : pp;
            int light_here = track_node_light_at(light, at, point);
            /* a piece with an open end has the open road's surface unless its texture is already the tunnel's dark one, and a corner of it on the tunnel's first node must meet the tunnel's own surface there: the light at which the texture looks as bright as the tunnel's floor (TRACK_NODE_LIGHT_MOUTH_FLOOR over the texture's own brightness), not the tunnel's */
            if (!in_tunnel && track_node_light_tunnel(light, at)) {
                const float lum = texture_lum(texture);
                const int road_cap = lum > (float)TRACK_NODE_LIGHT_MOUTH_FLOOR ? (int)(255.0f * (float)TRACK_NODE_LIGHT_MOUTH_FLOOR / lum) : 255;
                const int cap = point == 0 || point == 1 || point == 6 ? road_cap : point == 2 || point == 7 ? TRACK_NODE_LIGHT_WALL : 255;
                if (light_here > cap) light_here = cap;
            }
            piece.dark[k] = (float)((255.0 - light_here) * (MODERN_FOG_LEVELS - 1) / 255.0);
        }
        /* the piece's tables: its shallowest step's, in a tunnel when both its nodes are */
        const int a = step_id(light, from), b = step_id(light, to);
        const int id = a != 0 && (b == 0 || a < b) ? a : b;
        track_light_tunnel = (base_all >= 2 && !in_tunnel ? base_all : in_tunnel ? 1 : 0) | id << 3;
        const int32_t *const pc[4] = { piece.cam[0], piece.cam[1], piece.cam[2], piece.cam[3] };
        modern_road_cell_lit(q, view, texture, piece.uv, pc, transparent, lit_tables_for(texture), piece.dark);
    }
    track_light_tunnel = saved_tunnel;
    return 1;
}
