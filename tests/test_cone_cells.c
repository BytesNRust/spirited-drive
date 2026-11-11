/* The cone's cells (cone_cells.h): no cone, nothing; with one, a grid whose outer corners are the polygon's own, and a polygon small on the screen is left whole. */
#include "game/race/headlight_pool.h"
#include "game/race/track_node_light.h"
#include "game/viewer/cone_cells.h"
#include "tests/test.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    View view;
    FshEntry texture;
    int32_t cam[4][3];
    int32_t uv[8];
    double xz[4][2];
    uint32_t node[4];
    uint8_t corner[4];
} Poly;

/* a ground square 20 m on a side, 10 m ahead of a car at the origin facing +z; `depth` scales the camera-space z (a farther polygon is smaller on the screen) */
static Poly poly_of(int depth)
{
    Poly p;
    memset(&p, 0, sizeof p);
    view_init(&p.view, 0, 0, 320, 200, 90, 60);
    p.texture.width = 64, p.texture.height = 64;
    const double gx[4] = { -10, 10, 10, -10 }, gz[4] = { 10, 10, 30, 30 };
    for (int k = 0; k < 4; k++) {
        p.xz[k][0] = gx[k], p.xz[k][1] = gz[k];
        p.cam[k][0] = (int32_t)(gx[k] * 65536.0), p.cam[k][1] = 65536, p.cam[k][2] = (int32_t)(gz[k] * 65536.0) * depth;
        p.node[k] = (uint32_t)k;
        p.corner[k] = 200;
    }
    const int32_t uv[8] = { 0, 0, 64 << 16, 0, 64 << 16, 64 << 16, 0, 64 << 16 };
    memcpy(p.uv, uv, sizeof uv);
    return p;
}

static HeadlightPool cone_at(double x, double z, double hx, double hz)
{
    HeadlightPool h;
    memset(&h, 0, sizeof h);
    h.cone = 1, h.count = 1;
    h.cone_x[0] = x, h.cone_z[0] = z, h.cone_hx[0] = hx, h.cone_hz[0] = hz;
    return h;
}

static int plan(Poly *p, const TrackNodeLight *light, ConeGrid *g)
{
    const int32_t *cam[4] = { p->cam[0], p->cam[1], p->cam[2], p->cam[3] };
    return cone_cells_grid(g, &p->view, &p->texture, p->uv, cam, light, p->corner, p->xz, p->node);
}

static void test_without_a_cone_nothing_is_cut(void)
{
    Poly p = poly_of(1);
    ConeGrid g;
    memset(&g, 0x5a, sizeof g);
    CHECK_EQ_INT(plan(&p, NULL, &g), 0);                                   /* no light at all */
    TrackNodeLight light = { 0 };
    CHECK_EQ_INT(plan(&p, &light, &g), 0);                                 /* no pool */
    HeadlightPool h = cone_at(0, 0, 0, 1);
    h.cone = 0;
    light.pool = &h;
    CHECK_EQ_INT(plan(&p, &light, &g), 0);                                 /* a pool by nodes: the old path */
    CHECK_EQ_INT(((const uint8_t *)&g)[0], 0x5a);                          /* the grid was not touched */
    CHECK_EQ_INT(cone_cells_draw(NULL, &p.view, &p.texture, p.uv, (const int32_t *const[4]){ p.cam[0], p.cam[1], p.cam[2], p.cam[3] }, 0, NULL, &light, p.corner, p.xz, p.node), 0);
}

static void test_a_polygon_the_beam_does_not_touch_or_covers_is_left_whole(void)
{
    Poly p = poly_of(1);
    ConeGrid g;
    HeadlightPool far_away = cone_at(0, -500, 0, 1);                       /* the car is far behind: none of the nine points is lit */
    TrackNodeLight light = { .pool = &far_away };
    CHECK_EQ_INT(plan(&p, &light, &g), 0);
    HeadlightPool on_it = cone_at(0, 12, 0, 1);                            /* the nose over the square: the whole of it inside the full patch (12 m hold) */
    for (int k = 0; k < 4; k++) p.xz[k][0] *= 0.1, p.xz[k][1] = 12 + (p.xz[k][1] - 20) * 0.1;
    light.pool = &on_it;
    CHECK_EQ_INT(plan(&p, &light, &g), 0);
}

static void test_a_grid_whose_corners_are_the_polygons(void)
{
    Poly p = poly_of(1);
    HeadlightPool h = cone_at(0, 0, 0, 1);
    TrackNodeLight light = { .pool = &h };
    ConeGrid g;
    const int n = plan(&p, &light, &g);
    CHECK(n >= 2 && n <= CONE_CELLS_MAX);
    CHECK_EQ_INT(g.n, n);
    const int at[4][2] = { { 0, 0 }, { 0, n }, { n, n }, { n, 0 } };      /* (row j, column i) of the polygon's corners 0..3 */
    for (int k = 0; k < 4; k++) {
        for (int c = 0; c < 3; c++) CHECK_EQ_INT(g.cam[at[k][0]][at[k][1]][c], p.cam[k][c]);
        for (int c = 0; c < 2; c++) CHECK_EQ_INT(g.uv[at[k][0]][at[k][1]][c], p.uv[2 * k + c]);
    }
    for (int j = 0; j <= n; j++)                                           /* the dark is a level of the fog's tables */
        for (int i = 0; i <= n; i++) CHECK(g.dark[j][i] >= 0.0f && g.dark[j][i] <= (float)(MODERN_FOG_LEVELS - 1));
    const float lit = (255 - 200) * (MODERN_FOG_LEVELS - 1) / 255.0f;      /* the corners keep the byte they were given */
    for (int k = 0; k < 4; k++) CHECK(g.dark[at[k][0]][at[k][1]] > lit - 0.01f && g.dark[at[k][0]][at[k][1]] < lit + 0.01f);
    int graded = 0;                                                        /* and somewhere between them the beam's fading shows */
    for (int j = 0; j <= n; j++)
        for (int i = 0; i <= n; i++) graded |= g.dark[j][i] > lit + 0.5f || g.dark[j][i] < lit - 0.5f;
    CHECK(graded);
}

/* a polygon that is a quarter of its texture (a lane of a strip: uv 16..32 of 64) still carries its own corners' bytes at its corners: the light is placed by the polygon, not by uv across the whole texture
 * (which stopped it short of the corners, so the next polygon's edge began at another value: a hard step at every row) */
static void test_a_piece_of_a_texture_keeps_its_corners(void)
{
    Poly p = poly_of(1);
    const int32_t uv[8] = { 16 << 16, 0, 31 << 16, 0, 31 << 16, 63 << 16, 16 << 16, 63 << 16 };
    memcpy(p.uv, uv, sizeof uv);
    p.corner[0] = 250, p.corner[1] = 60, p.corner[2] = 60, p.corner[3] = 250;
    HeadlightPool h = cone_at(0, 0, 0, 1);
    TrackNodeLight light = { .pool = &h };
    ConeGrid g;
    const int n = plan(&p, &light, &g);
    CHECK(n >= 2);
    const int at[4][2] = { { 0, 0 }, { 0, n }, { n, n }, { n, 0 } };
    for (int k = 0; k < 4; k++) {
        const float want = (255 - p.corner[k]) * (MODERN_FOG_LEVELS - 1) / 255.0f;
        CHECK(g.dark[at[k][0]][at[k][1]] > want - 1.0f && g.dark[at[k][0]][at[k][1]] < want + 1.0f);   /* within the beam's own change at that corner */
    }
}

static void test_a_polygon_small_on_the_screen_is_not_cut(void)
{
    Poly p = poly_of(1000);                                                /* the same ground, a thousand times farther in the view: a few pixels */
    HeadlightPool h = cone_at(0, 0, 0, 1);
    TrackNodeLight light = { .pool = &h };
    ConeGrid g;
    CHECK_EQ_INT(plan(&p, &light, &g), 0);
    p = poly_of(1);
    p.cam[0][2] = 0;                                                       /* a corner at the lens: as big as can be, cut */
    CHECK(plan(&p, &light, &g) >= 2);
}

int main(void)
{
    setenv("TNFS_CELL_ON", "1", 1);   /* the cells are opt-in (cached at the first plan) */
    test_without_a_cone_nothing_is_cut();
    test_a_polygon_the_beam_does_not_touch_or_covers_is_left_whole();
    test_a_grid_whose_corners_are_the_polygons();
    test_a_piece_of_a_texture_keeps_its_corners();
    test_a_polygon_small_on_the_screen_is_not_cut();
    return TEST_REPORT();
}
