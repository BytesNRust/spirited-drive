/* port: the L buildings closed into boxes. A two-sided board (descriptor type 6) is a front and a side meeting at a corner; seen from behind or inside (the 360
 * camera, reverse) it is open. With the flag, and Modern 3D's camera-space draw, the two missing walls are added as the opposite faces, unmirrored, and a roof;
 * without the flag, or on the exe's mapper, it is drawn as it always was. */
#include "game/race/track_object_draw.h"
#include "test.h"

#include <string.h>

typedef struct {
    int camera, roof, full, alt;
    int32_t texture[8];
    int32_t cam[8][4][3];   /* the corners of each camera-space draw */
    int32_t roof_cam[4][3];
} Calls;

static void c_model(void *u, int32_t m, const int32_t p[3], int32_t a) { (void)u, (void)m, (void)p, (void)a; }
static void c_full(void *u, int32_t t, const int32_t s[8]) { (void)t, (void)s, ((Calls *)u)->full++; }
static void c_alt(void *u, int32_t t, const int32_t s[8]) { (void)t, (void)s, ((Calls *)u)->alt++; }
static void c_camera(void *u, int32_t t, const int32_t *const cam[4])
{
    Calls *c = u;
    if (c->camera < 8) {
        c->texture[c->camera] = t;
        for (int i = 0; i < 4; i++) memcpy(c->cam[c->camera][i], cam[i], sizeof c->cam[0][0]);
    }
    c->camera++;
}
static void c_roof(void *u, int32_t t, const int32_t *const cam[4])
{
    Calls *c = u;
    (void)t;
    c->roof++;
    for (int i = 0; i < 4; i++) memcpy(c->roof_cam[i], cam[i], sizeof c->roof_cam[0]);
}

/* a box seen from the front-right: the front's corners 0..3 (top-left, top-right, bottom-right, bottom-left), then 4 and 5 the side's far edge (top, bottom) */
static const int32_t BOX[6][3] = { { -300, 200, 500 }, { 300, 200, 500 }, { 300, -200, 500 }, { -300, -200, 500 }, { 300, 200, 900 }, { 300, -200, 900 } };
static const int32_t SCREEN[6][2] = { { 10, 10 }, { 60, 10 }, { 60, 50 }, { 10, 50 }, { 80, 12 }, { 80, 48 } };

static Calls draw(int flag, int with_camera, int with_roof)
{
    uint8_t descriptor[TRACK_WALK_RECORD_SIZE] = { 0 }, record[TRACK_WALK_RECORD_SIZE] = { 0 };
    descriptor[1] = 6, descriptor[2] = 8, descriptor[3] = 12;   /* the front's and the side's textures */
    Calls c = { 0 };
    TrackObjectWalk walk = { descriptor, record, 1, 0, 1, 6, 6, 0, 0, 6, 0, 0, 0 };
    TrackObjectDraw d = { &walk, NULL, NULL, BOX, SCREEN, 6, 0, 0, { &c, c_model, c_full, c_alt, with_camera ? c_camera : NULL, NULL, with_roof ? c_roof : NULL } };
    track_port_close_boxes = flag;
    track_object_draw(&d, 10, 1);
    track_port_close_boxes = 0;
    return c;
}

static void test_without_the_flag_it_is_the_l(void)
{
    const Calls c = draw(0, 1, 1);
    CHECK_EQ_INT(c.camera, 2);   /* the front and the side, as always */
    CHECK_EQ_INT(c.roof, 0);
}

static void test_the_exes_mapper_is_left_alone(void)
{
    const Calls c = draw(1, 0, 1);
    CHECK_EQ_INT(c.camera, 0);
    CHECK_EQ_INT(c.alt, 2);      /* no camera-space draw: the flag does nothing */
    CHECK_EQ_INT(c.roof, 0);
}

static void test_the_flag_closes_the_box(void)
{
    const Calls c = draw(1, 1, 1);
    CHECK_EQ_INT(c.camera, 4);   /* front, side, then the back and the left wall */
    CHECK_EQ_INT(c.roof, 1);
    CHECK_EQ_INT(c.texture[2], c.texture[0]);   /* the back is the front's texture ... */
    CHECK_EQ_INT(c.texture[3], c.texture[1]);   /* ... the left wall the side's */
    /* the far corners the walls share: the left wall's far top is the front's top-left carried by the side's depth */
    const int32_t far_top_left[3] = { BOX[0][0] + (BOX[4][0] - BOX[1][0]), BOX[0][1] + (BOX[4][1] - BOX[1][1]), BOX[0][2] + (BOX[4][2] - BOX[1][2]) };
    const int32_t far_bottom_left[3] = { BOX[3][0] + (BOX[5][0] - BOX[2][0]), BOX[3][1] + (BOX[5][1] - BOX[2][1]), BOX[3][2] + (BOX[5][2] - BOX[2][2]) };
    CHECK(memcmp(c.cam[2][0], BOX[4], sizeof far_top_left) == 0);               /* the back, seen from outside: its left is the side's far edge */
    CHECK(memcmp(c.cam[2][1], far_top_left, sizeof far_top_left) == 0);
    CHECK(memcmp(c.cam[2][2], far_bottom_left, sizeof far_top_left) == 0);
    CHECK(memcmp(c.cam[2][3], BOX[5], sizeof far_top_left) == 0);
    CHECK(memcmp(c.cam[3][0], far_top_left, sizeof far_top_left) == 0);         /* the left wall from outside: far edge, then the front's left edge */
    CHECK(memcmp(c.cam[3][1], BOX[0], sizeof far_top_left) == 0);
    CHECK(memcmp(c.cam[3][2], BOX[3], sizeof far_top_left) == 0);
    CHECK(memcmp(c.cam[3][3], far_bottom_left, sizeof far_top_left) == 0);
    CHECK(memcmp(c.roof_cam[0], far_top_left, sizeof far_top_left) == 0);       /* the roof from above: far left, far right, near right, near left */
    CHECK(memcmp(c.roof_cam[1], BOX[4], sizeof far_top_left) == 0);
    CHECK(memcmp(c.roof_cam[2], BOX[1], sizeof far_top_left) == 0);
    CHECK(memcmp(c.roof_cam[3], BOX[0], sizeof far_top_left) == 0);
}

static void test_no_roof_op_still_gets_its_walls(void)
{
    const Calls c = draw(1, 1, 0);
    CHECK_EQ_INT(c.camera, 4);
    CHECK_EQ_INT(c.roof, 0);
}

int main(void)
{
    test_without_the_flag_it_is_the_l();
    test_the_exes_mapper_is_left_alone();
    test_the_flag_closes_the_box();
    test_no_roof_op_still_gets_its_walls();
    return TEST_REPORT();
}
