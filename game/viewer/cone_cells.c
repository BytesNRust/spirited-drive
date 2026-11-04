#include "game/viewer/cone_cells.h"

#include "game/race/headlight_pool.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

enum { MAX_GRID = 6, SPREAD = 24, WIDE_M = 8 };
static long cells_drawn, polygons_drawn;
static int stats_registered;

static void print_stats(void)
{
    if (getenv("TNFS_CELL_STATS") != NULL) fprintf(stderr, "cone_cells: %ld polygons, %ld cells\n", polygons_drawn, cells_drawn);
}

void cone_cells_stats(long *cells, long *polygons)
{
    *cells = cells_drawn;
    *polygons = polygons_drawn;
}

static double lerp(double a, double b, double t) { return a + (b - a) * t; }

int cone_cells_draw(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8], const int32_t *const cam[4], int transparent,
                    const uint8_t (*tables)[256], const PsxLight *light, const uint8_t corner[4], const double xz[4][2])
{
    if (light == NULL || light->pool == NULL || !light->pool->cone) return 0;
    static int off = -1;
    if (off < 0) off = getenv("TNFS_CELL_OFF") != NULL;   /* dev: the cells off, for the before / after */
    if (off) return 0;
    int lift[4], lo = 256, hi = 0;
    for (int k = 0; k < 4; k++) {
        lift[k] = headlight_pool_lift_xz(light->pool, xz[k][0], xz[k][1]);
        if (lift[k] < lo) lo = lift[k];
        if (lift[k] > hi) hi = lift[k];
    }
    double extent = 0;
    for (int k = 0; k < 4; k++) {
        const double d = hypot(xz[k][0] - xz[(k + 1) % 4][0], xz[k][1] - xz[(k + 1) % 4][1]);
        if (d > extent) extent = d;
    }
    /* the polygon is fine as it is when the beam does not touch it, or is the same all over it, or is small */
    if ((hi == 0 && lo == 0) || (hi - lo < SPREAD && extent < WIDE_M) || extent < 3.0) return 0;
    int n = (int)ceil(extent / 4.0);
    if (n < 2) n = 2;
    if (n > MAX_GRID) n = MAX_GRID;
    /* the open night's gain at each corner, to take the row's own byte back out of the light the corner carries */
    double base[4];
    for (int k = 0; k < 4; k++) base[k] = corner[k] * 256.0 / psx_light_open_gain(light, lift[k]);
    const double w = (double)(int16_t)texture->width * 65536.0, h = (double)(int16_t)texture->height * 65536.0;
    if (!stats_registered) stats_registered = 1, atexit(print_stats);
    polygons_drawn++;
    int32_t pc[MAX_GRID + 1][MAX_GRID + 1][3];
    int32_t puv[MAX_GRID + 1][MAX_GRID + 1][2];
    float dark[MAX_GRID + 1][MAX_GRID + 1];
    for (int j = 0; j <= n; j++)
        for (int i = 0; i <= n; i++) {
            const double s = (double)i / n, t = (double)j / n;
            for (int c = 0; c < 3; c++) pc[j][i][c] = (int32_t)lrint(lerp(lerp(cam[0][c], cam[1][c], s), lerp(cam[3][c], cam[2][c], s), t));
            for (int c = 0; c < 2; c++) puv[j][i][c] = (int32_t)lrint(lerp(lerp(uv[c], uv[2 + c], s), lerp(uv[6 + c], uv[4 + c], s), t));
            const double a = fmin(fmax(puv[j][i][0] / w, 0), 1), b = fmin(fmax(puv[j][i][1] / h, 0), 1);
            double x = lerp(lerp(xz[0][0], xz[1][0], a), lerp(xz[3][0], xz[2][0], a), b);
            double z = lerp(lerp(xz[0][1], xz[1][1], a), lerp(xz[3][1], xz[2][1], a), b);
            const double bv = lerp(lerp(base[0], base[1], a), lerp(base[3], base[2], a), b);
            double v = bv * psx_light_open_gain(light, headlight_pool_lift_xz(light->pool, x, z)) / 256.0;
            if (v > 255.0) v = 255.0;
            dark[j][i] = (float)((255.0 - v) * (MODERN_FOG_LEVELS - 1) / 255.0);
        }
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            const int idx[4][2] = { { j, i }, { j, i + 1 }, { j + 1, i + 1 }, { j + 1, i } };
            int32_t cc[4][3];
            const int32_t *cp[4];
            int32_t cuv[8];
            float cd[4];
            for (int k = 0; k < 4; k++) {
                for (int c = 0; c < 3; c++) cc[k][c] = pc[idx[k][0]][idx[k][1]][c];
                cp[k] = cc[k];
                cuv[2 * k] = puv[idx[k][0]][idx[k][1]][0], cuv[2 * k + 1] = puv[idx[k][0]][idx[k][1]][1];
                cd[k] = dark[idx[k][0]][idx[k][1]];
            }
            modern_road_cell_lit(q, view, texture, cuv, cp, transparent, tables, cd);
            cells_drawn++;
        }
    return 1;
}
