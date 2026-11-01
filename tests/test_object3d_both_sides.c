/* An open surface drawn from behind too (object3d_both_sides): a polygon with an edge no other polygon shares loses its culling, those of a closed
 * solid keep it. */
#include "game/gfx/object3d.h"
#include "test.h"

static void test_a_board_is_open(void)
{
    Object3DPolygon polygons[1] = { { .flags = 4, .vertex = { 0, 1, 2, 3 } } };
    Object3D obj = { 0 };
    obj.polygons = polygons, obj.polygon_count = 1, obj.order_count = 1;
    CHECK_EQ_INT(object3d_both_sides(&obj), 1);
    CHECK(polygons[0].flags & POLY_NO_CULL);
    CHECK_EQ_INT(object3d_both_sides(&obj), 0);   /* already done */
}

static void test_a_closed_solid_keeps_its_culling(void)
{
    /* a tetrahedron: four triangles, every edge shared by two */
    Object3DPolygon polygons[4] = { { .flags = 3, .vertex = { 0, 1, 2 } }, { .flags = 3, .vertex = { 0, 3, 1 } },
                                    { .flags = 3, .vertex = { 1, 3, 2 } }, { .flags = 3, .vertex = { 2, 3, 0 } } };
    Object3D obj = { 0 };
    obj.polygons = polygons, obj.polygon_count = 4, obj.order_count = 4;
    CHECK_EQ_INT(object3d_both_sides(&obj), 0);
    for (int i = 0; i < 4; i++) CHECK(!(polygons[i].flags & POLY_NO_CULL));
}

static void test_an_arch_is_open_where_its_edges_are(void)
{
    /* three quads in a row sharing their side edges (an inverted U): the two outer ones have an open outer edge, so does the middle (top and bottom) */
    Object3DPolygon polygons[3] = { { .flags = 4, .vertex = { 0, 1, 5, 4 } }, { .flags = 4, .vertex = { 1, 2, 6, 5 } },
                                    { .flags = 4, .vertex = { 2, 3, 7, 6 } } };
    Object3D obj = { 0 };
    obj.polygons = polygons, obj.polygon_count = 3, obj.order_count = 3;
    CHECK_EQ_INT(object3d_both_sides(&obj), 3);
}

static void test_flags_that_already_draw_both_ways_and_lines_are_left(void)
{
    Object3DPolygon polygons[3] = { { .flags = 4 | POLY_TWO_SIDED, .vertex = { 0, 1, 2, 3 } }, { .flags = 2, .vertex = { 4, 5 } },
                                    { .flags = 4 | POLY_REVERSED, .vertex = { 8, 9, 10, 11 } } };
    Object3D obj = { 0 };
    obj.polygons = polygons, obj.polygon_count = 3, obj.order_count = 3;
    CHECK_EQ_INT(object3d_both_sides(&obj), 1);   /* only the reversed board */
    CHECK(!(polygons[1].flags & POLY_NO_CULL));
    CHECK(polygons[2].flags & POLY_REVERSED);     /* the mirror's flag is kept */
}

int main(void)
{
    test_a_board_is_open();
    test_a_closed_solid_keeps_its_culling();
    test_an_arch_is_open_where_its_edges_are();
    test_flags_that_already_draw_both_ways_and_lines_are_left();
    return TEST_REPORT();
}
