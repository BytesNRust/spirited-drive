/* A model on a mirrored track (object3d_mirror): the exe flips POLY_REVERSED on the first order_count polygons of
 * the cars (00457d40) and of the scenery models (004576f0), so their one-sided faces survive the camera's x flip. */
#include "game/gfx/object3d.h"
#include "test.h"

static void test_the_ordered_polygons_turn_round(void)
{
    Object3DPolygon polygons[3] = { 0 };
    polygons[0].flags = POLY_REVERSED | 1;
    polygons[1].flags = 2;
    polygons[2].flags = 4;
    Object3D obj = { 0 };
    obj.polygons = polygons, obj.polygon_count = 3, obj.order_count = 2;
    object3d_mirror(&obj);
    CHECK_EQ_INT(polygons[0].flags, 1);
    CHECK_EQ_INT(polygons[1].flags, POLY_REVERSED | 2);
    CHECK_EQ_INT(polygons[2].flags, 4);   /* past order_count: left */
    object3d_mirror(&obj);
    CHECK_EQ_INT(polygons[0].flags, POLY_REVERSED | 1);
}

int main(void)
{
    test_the_ordered_polygons_turn_round();
    return TEST_REPORT();
}
