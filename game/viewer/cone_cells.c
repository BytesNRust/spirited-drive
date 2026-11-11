#include "game/viewer/cone_cells.h"

#include "game/race/headlight_pool.h"

#include <math.h>
#include <stdlib.h>

enum { SPREAD = 24, WIDE_M = 8, MIN_PIXELS = 8 };

static double lerp(double a, double b, double t) { return a + (b - a) * t; }

/* the polygon's longest side on the screen, pixels: the camera-space corners projected by the view's focal length; a corner at or behind the lens makes it as big as can be */
static double screen_extent(const View *view, const int32_t *const cam[4])
{
    double sx[4], sy[4];
    for (int k = 0; k < 4; k++) {
        if (cam[k][2] <= 0) return 1e9;
        sx[k] = (double)cam[k][0] * (double)view->focal_x / (double)cam[k][2];
        sy[k] = (double)cam[k][1] * (double)view->focal_x / (double)cam[k][2];
    }
    double best = 0;
    for (int k = 0; k < 4; k++) {
        const double d = hypot(sx[k] - sx[(k + 1) % 4], sy[k] - sy[(k + 1) % 4]);
        if (d > best) best = d;
    }
    return fabs(best);
}

/* The grid's points: the camera space, the uv and the light's darkness of each, from the corners'. */
static void cone_grid_fill(ConeGrid *g, int n, const int32_t uv[8], const int32_t *const cam[4], const double xz[4][2], const double base[4],
                           const double lamp[4], const TrackNodeLight *light)
{
    g->n = n;
    for (int j = 0; j <= n; j++)
        for (int i = 0; i <= n; i++) {
            const double s = (double)i / n, t = (double)j / n;
            for (int c = 0; c < 3; c++) g->cam[j][i][c] = (int32_t)lrint(lerp(lerp(cam[0][c], cam[1][c], s), lerp(cam[3][c], cam[2][c], s), t));
            for (int c = 0; c < 2; c++) g->uv[j][i][c] = (int32_t)lrint(lerp(lerp(uv[c], uv[2 + c], s), lerp(uv[6 + c], uv[4 + c], s), t));
            const double a = s, b = t;   /* the place in the polygon (uv is only its piece of the texture) */
            const double x = lerp(lerp(xz[0][0], xz[1][0], a), lerp(xz[3][0], xz[2][0], a), b);
            const double z = lerp(lerp(xz[0][1], xz[1][1], a), lerp(xz[3][1], xz[2][1], a), b);
            const double bv = lerp(lerp(base[0], base[1], a), lerp(base[3], base[2], a), b);
            const double lf = lerp(lerp(lamp[0], lamp[1], a), lerp(lamp[3], lamp[2], a), b);
            double v = bv * track_node_light_open_gain_floor(light, headlight_pool_lift_xz(light->pool, x, z), (int)lf) / 256.0;
            if (v > 255.0) v = 255.0;
            g->dark[j][i] = (float)((255.0 - v) * (MODERN_FOG_LEVELS - 1) / 255.0);
        }
}

int cone_cells_grid(ConeGrid *g, const View *view, const FshEntry *texture, const int32_t uv[8], const int32_t *const cam[4], const TrackNodeLight *light, const uint8_t corner[4],
                    const double xz[4][2], const uint32_t node[4])
{
    (void)texture;   /* the polygon's own corners: its piece of the texture does not place the light */
    if (light == NULL || light->pool == NULL || !light->pool->cone) return 0;
    static int on = -1;
    if (on < 0) on = getenv("TNFS_CELL_ON") != NULL;   /* dev: the cells are off by default (+5..8 % a frame at night on CY1); on to see them */
    if (!on) return 0;
    /* the beam at the four corners, the four edges' middles and the centre (a polygon bigger than the patch can have the beam inside and none at its corners) */
    int lift[4], lo = 256, hi = 0;
    for (int k = 0; k < 4; k++) {
        lift[k] = headlight_pool_lift_xz(light->pool, xz[k][0], xz[k][1]);
        if (lift[k] < lo) lo = lift[k];
        if (lift[k] > hi) hi = lift[k];
    }
    for (int k = 0; k < 5; k++) {
        const double x = k < 4 ? (xz[k][0] + xz[(k + 1) % 4][0]) / 2 : (xz[0][0] + xz[1][0] + xz[2][0] + xz[3][0]) / 4;
        const double z = k < 4 ? (xz[k][1] + xz[(k + 1) % 4][1]) / 2 : (xz[0][1] + xz[1][1] + xz[2][1] + xz[3][1]) / 4;
        const int l = headlight_pool_lift_xz(light->pool, x, z);
        if (l < lo) lo = l;
        if (l > hi) hi = l;
    }
    if (hi == 0 || lo == 256) return 0;   /* the beam is on none of the nine points, or on all of them whole: nothing to grade */
    double extent = 0;
    for (int k = 0; k < 4; k++) {
        const double d = hypot(xz[k][0] - xz[(k + 1) % 4][0], xz[k][1] - xz[(k + 1) % 4][1]);
        if (d > extent) extent = d;
    }
    /* the polygon is fine as it is when the light is the same all over it, or it is small */
    if ((hi - lo < SPREAD && extent < WIDE_M) || extent < 3.0) return 0;
    const double pixels = screen_extent(view, cam);
    if (pixels < MIN_PIXELS) return 0;
    int n = (int)ceil(extent / 4.0);
    const int by_screen = (int)(pixels / CONE_CELLS_PIXELS);   /* a cell is at least CONE_CELLS_PIXELS across on the screen */
    if (n > by_screen) n = by_screen;
    if (n < 2) n = 2;
    if (n > CONE_CELLS_MAX) n = CONE_CELLS_MAX;
    /* the open night's gain at each corner, to take the row's own byte back out of the light the corner carries; and the lamp's floor there (lamp_pool_bright), interpolated like the rest */
    double base[4], lamp[4];
    for (int k = 0; k < 4; k++) {
        lamp[k] = track_node_light_lamp_floor(light, node[k]);
        base[k] = corner[k] * 256.0 / track_node_light_open_gain_floor(light, lift[k], (int)lamp[k]);
    }
    cone_grid_fill(g, n, uv, cam, xz, base, lamp, light);
    return n;
}

int cone_cells_draw(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8], const int32_t *const cam[4], int transparent,
                    const uint8_t (*tables)[256], const TrackNodeLight *light, const uint8_t corner[4], const double xz[4][2], const uint32_t node[4])
{
    ConeGrid g;
    const int n = cone_cells_grid(&g, view, texture, uv, cam, light, corner, xz, node);
    if (n == 0) return 0;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            const int idx[4][2] = { { j, i }, { j, i + 1 }, { j + 1, i + 1 }, { j + 1, i } };
            int32_t cuv[8];
            const int32_t *cp[4];
            float cd[4];
            for (int k = 0; k < 4; k++) {
                cp[k] = g.cam[idx[k][0]][idx[k][1]];
                cuv[2 * k] = g.uv[idx[k][0]][idx[k][1]][0], cuv[2 * k + 1] = g.uv[idx[k][0]][idx[k][1]][1];
                cd[k] = g.dark[idx[k][0]][idx[k][1]];
            }
            modern_road_cell_lit(q, view, texture, cuv, cp, transparent, tables, cd);
        }
    return 1;
}
