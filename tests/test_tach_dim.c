/* port (tach_dim): which pixels of the outside views' tachometer are the dial's face (tach_dim.h): inside the shape's box, not the needle, not see-through. */
#include "game/race/tach_dim.h"
#include "test.h"

#include <string.h>

enum { KEY = 0xff, NEEDLE = 0x40 };

/* an FSH shape header: width 20, height 10 at (30, 400) on the 640 view */
static void make_shape(uint8_t shape[0x10])
{
    memset(shape, 0, 0x10);
    shape[4] = 20, shape[6] = 10;
    shape[0xc] = 30 & 0xff, shape[0xd] = 0;
    shape[0xe] = 400 & 0xff, shape[0xf] = 400 >> 8;
}

static void test_the_box_is_the_shapes(void)
{
    uint8_t shape[0x10];
    make_shape(shape);
    int box[4];
    CHECK(tach_dim_box(shape, box));
    CHECK_EQ_INT(box[0], 30);
    CHECK_EQ_INT(box[1], 400);
    CHECK_EQ_INT(box[2], 20);
    CHECK_EQ_INT(box[3], 10);
    CHECK(!tach_dim_box(NULL, box));
    shape[4] = 0;
    CHECK(!tach_dim_box(shape, box));   /* an empty shape has no face */
}

static void test_the_face_is_inside_and_not_the_needle(void)
{
    uint8_t shape[0x10];
    make_shape(shape);
    CHECK(tach_dim_face(shape, 30, 400, 7, NEEDLE, KEY));       /* the corner */
    CHECK(tach_dim_face(shape, 49, 409, 7, NEEDLE, KEY));       /* the opposite corner */
    CHECK(!tach_dim_face(shape, 50, 409, 7, NEEDLE, KEY));      /* just right of it */
    CHECK(!tach_dim_face(shape, 49, 410, 7, NEEDLE, KEY));      /* just under it */
    CHECK(!tach_dim_face(shape, 29, 400, 7, NEEDLE, KEY));
    CHECK(!tach_dim_face(shape, 30, 399, 7, NEEDLE, KEY));
    CHECK(!tach_dim_face(shape, 40, 405, NEEDLE, NEEDLE, KEY));   /* the needle keeps its full light */
    CHECK(!tach_dim_face(shape, 40, 405, KEY, NEEDLE, KEY));      /* see-through is not drawn */
    CHECK(!tach_dim_face(NULL, 40, 405, 7, NEEDLE, KEY));        /* no tach, no face */
}

int main(void)
{
    test_the_box_is_the_shapes();
    test_the_face_is_inside_and_not_the_needle();
    return TEST_REPORT();
}
