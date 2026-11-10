/* The tunnel polygons' cells (tunnel_cells.h): how many a polygon is cut into and where each lies. */
#include "game/viewer/tunnel_cells.h"
#include "tests/test.h"

#include <math.h>
#include <string.h>

static void test_plan_reads_the_long_sides_from_the_nodes(void)
{
    int along = -1;
    const uint32_t a[4] = { 1076, 1072, 1072, 1076 };   /* corners 0-1 and 3-2 run along the road: 4 nodes */
    CHECK_EQ_INT(tunnel_cells_plan(a, &along), 4);
    CHECK_EQ_INT(along, 0);
    const uint32_t b[4] = { 1072, 1072, 1076, 1076 };   /* the other way round */
    CHECK_EQ_INT(tunnel_cells_plan(b, &along), 4);
    CHECK_EQ_INT(along, 1);
    const uint32_t one[4] = { 1099, 1098, 1098, 1099 };
    CHECK_EQ_INT(tunnel_cells_plan(one, &along), 1);
    const uint32_t long_run[4] = { 1120, 1100, 1100, 1120 };
    CHECK_EQ_INT(tunnel_cells_plan(long_run, &along), TUNNEL_CELLS_MAX);   /* the most */
    const uint32_t seam[4] = { 1919, 1, 1, 1919 };   /* a ring's seam is not cut */
    CHECK_EQ_INT(tunnel_cells_plan(seam, &along), 0);
    const uint32_t mixed[4] = { 1, 2, 3, 4 };
    CHECK_EQ_INT(tunnel_cells_plan(mixed, &along), 0);
}

/* a flat polygon: corner 0 at (0,0), 1 at (8,0) along the road, 2 at (8,6), 3 at (0,6): the long sides 0-1 and 3-2 */
static void test_pieces_tile_the_polygon(void)
{
    const int32_t c0[3] = { 0, 0, 100 }, c1[3] = { 8000, 0, 100 }, c2[3] = { 8000, 6000, 100 }, c3[3] = { 0, 6000, 100 };
    const int32_t *const cam[4] = { c0, c1, c2, c3 };
    const int32_t uv[8] = { 0, 0, 800, 0, 800, 600, 0, 600 };
    const double xz[4][2] = { { 0, 0 }, { 8, 0 }, { 8, 6 }, { 0, 6 } };
    const float dark[4] = { 0, 4, 4, 0 };
    TunnelPiece p;
    tunnel_cells_piece(&p, 0, 4, 2, 1, 0, uv, cam, xz, dark);   /* the second of 4 along, the first of 2 across */
    CHECK_EQ_INT(p.cam[0][0], 2000);
    CHECK_EQ_INT(p.cam[1][0], 4000);
    CHECK_EQ_INT(p.cam[2][1], 3000);
    CHECK_EQ_INT(p.cam[0][1], 0);
    CHECK_EQ_INT(p.uv[0], 200);
    CHECK_EQ_INT(p.uv[2], 400);
    CHECK(fabs(p.xz[2][0] - 4.0) < 1e-9 && fabs(p.xz[2][1] - 3.0) < 1e-9);
    CHECK(fabsf(p.dark[0] - 1.0f) < 1e-6f && fabsf(p.dark[1] - 2.0f) < 1e-6f);
    CHECK(fabs(p.s[0] - 0.25) < 1e-9 && fabs(p.s[1] - 0.5) < 1e-9);
    /* the same polygon with its corners turned: the long sides are 0-3 and 1-2; the cell keeps the polygon's corner order */
    const int32_t *const turned[4] = { c0, c3, c2, c1 };   /* corner 1 is (0,6), 3 is (8,0): 0-3 and 1-2 run along x */
    const int32_t tuv[8] = { 0, 0, 0, 600, 800, 600, 800, 0 };
    const double txz[4][2] = { { 0, 0 }, { 0, 6 }, { 8, 6 }, { 8, 0 } };
    const float tdark[4] = { 0, 0, 4, 4 };
    tunnel_cells_piece(&p, 1, 4, 2, 1, 0, tuv, turned, txz, tdark);
    CHECK_EQ_INT(p.cam[0][0], 2000);   /* corner 0: along 2 of 8 at the first side */
    CHECK_EQ_INT(p.cam[3][0], 4000);   /* corner 3: the same side, the next piece's start */
    CHECK_EQ_INT(p.cam[2][1], 3000);
    CHECK(fabs(p.s[0] - 0.25) < 1e-9 && fabs(p.s[3] - 0.5) < 1e-9);
}

int main(void)
{
    test_plan_reads_the_long_sides_from_the_nodes();
    test_pieces_tile_the_polygon();
    return TEST_REPORT();
}
