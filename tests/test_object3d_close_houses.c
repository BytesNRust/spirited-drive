/* close_houses (object3d_close_houses): TR3's village house (model 4) and long building (model 3) get the faces the road never showed; other objects are left. */
#include "game/gfx/object3d.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

typedef struct { uint32_t flags, v[4], uv[4]; } Src;

static void make(Object3D *o, const int32_t (*verts)[3], uint32_t nv, const Src *polys, uint32_t np)
{
    memset(o, 0, sizeof *o);
    o->model = malloc(nv * sizeof *o->model), o->camera = malloc(nv * sizeof *o->camera), o->screen = malloc(nv * sizeof *o->screen);
    o->polygons = malloc(np * sizeof *o->polygons), o->order = malloc(np * sizeof *o->order);
    memcpy(o->model, verts, nv * sizeof *o->model);
    o->vertex_count = nv, o->polygon_count = np, o->order_count = np;
    for (uint32_t i = 0; i < np; i++) {
        o->polygons[i].flags = polys[i].flags, o->order[i] = i, o->polygons[i].link = UINT32_MAX;
        for (int k = 0; k < 4; k++) o->polygons[i].vertex[k] = polys[i].v[k], o->polygons[i].uv[k] = polys[i].uv[k];
    }
}

static void house(Object3D *o)
{
    /* model 4 of TR3_T01.FAM: a left wall, the front wall, the front roof slope */
    static const int32_t V[10][3] = { { 0, 264, 240 }, { 0, 264, 0 }, { 0, 0, 0 }, { 0, 0, 240 }, { 0, 168, 0 },
                                      { 312, 0, 0 }, { 312, 168, 0 }, { 0, 264, 120 }, { 312, 264, 120 }, { 0, 168, 0 } };
    static const Src P[3] = { { 0x8c, { 0, 1, 2, 3 }, { 10, 11, 12, 13 } }, { 0x84, { 4, 2, 5, 6 }, { 20, 21, 22, 23 } },
                              { 0x84, { 7, 4, 6, 8 }, { 30, 31, 32, 33 } } };
    make(o, V, 10, P, 3);
}

static void test_the_house_gets_three_faces_and_keeps_the_source_look(void)
{
    Object3D o;
    house(&o);
    CHECK_EQ_INT(object3d_close_houses(&o), 3);
    CHECK_EQ_INT((int)o.polygon_count, 6);
    CHECK_EQ_INT((int)o.order_count, 6);
    CHECK_EQ_INT((int)o.order[5], 5);
    CHECK_EQ_INT((int)o.polygons[3].flags, 0x8c);   /* the colour key and flags of the source */
    CHECK_EQ_INT((int)o.polygons[4].flags, 0x84);
    CHECK_EQ_INT((int)o.polygons[5].flags, 0x84);
    CHECK_EQ_INT((int)o.polygons[3].uv[0], 10);      /* first corner keeps its uv, the rest run the other way */
    CHECK_EQ_INT((int)o.polygons[3].uv[1], 13);
    for (uint32_t k = 0; k < o.vertex_count; k++) CHECK(o.model[k][0] >= 0 && o.model[k][0] <= 312 && o.model[k][2] >= 0 && o.model[k][2] <= 240);
    CHECK_EQ_INT(object3d_close_houses(&o), 0);      /* closed once: the box and counts no longer match */
    object3d_free(&o);
}

static void test_the_right_wall_is_the_left_one_across_the_house(void)
{
    Object3D o;
    house(&o);
    object3d_close_houses(&o);
    const Object3DPolygon *w = &o.polygons[3];
    for (int k = 0; k < 4; k++) CHECK_EQ_INT(o.model[w->vertex[k]][0], 312);
    CHECK_EQ_INT(o.model[w->vertex[0]][1], 264);
    CHECK_EQ_INT(o.model[w->vertex[0]][2], 240);
    object3d_free(&o);
}

static void test_the_long_building_gets_its_far_end_cap(void)
{
    static const int32_t V[16][3] = { { 0, 150, 1500 }, { 0, 150, 0 }, { 0, 0, 0 }, { 0, 0, 1500 }, { 225, 150, 1500 }, { 225, 150, 0 }, { 225, 0, 0 }, { 225, 0, 1500 },
                                      { 225, 126, 1500 }, { 225, 126, 0 }, { 0, 126, 0 }, { 0, 126, 1500 }, { 225, 90, 0 }, { 0, 90, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
    static const Src P[4] = { { 0x8c, { 0, 1, 2, 3 }, { 0 } }, { 0x8c, { 4, 5, 6, 7 }, { 0 } }, { 0x84, { 8, 9, 10, 11 }, { 0 } }, { 0x84, { 10, 9, 12, 13 }, { 0 } } };
    Object3D o;
    make(&o, V, 16, P, 4);
    CHECK_EQ_INT(object3d_close_houses(&o), 1);
    CHECK_EQ_INT((int)o.polygon_count, 5);
    CHECK_EQ_INT((int)o.polygons[4].flags, 0x84);
    for (int k = 0; k < 4; k++) CHECK_EQ_INT(o.model[o.polygons[4].vertex[k]][2], 1500);
    object3d_free(&o);
}

static void test_other_objects_are_left_alone(void)
{
    static const int32_t V[4][3] = { { 0, 0, 0 }, { 10, 0, 0 }, { 10, 10, 0 }, { 0, 10, 0 } };
    static const Src P[1] = { { 0x84, { 0, 1, 2, 3 }, { 0 } } };
    Object3D o;
    make(&o, V, 4, P, 1);
    CHECK_EQ_INT(object3d_close_houses(&o), 0);
    CHECK_EQ_INT((int)o.polygon_count, 1);
    object3d_free(&o);
}

int main(void)
{
    test_the_house_gets_three_faces_and_keeps_the_source_look();
    test_the_right_wall_is_the_left_one_across_the_house();
    test_the_long_building_gets_its_far_end_cap();
    test_other_objects_are_left_alone();
    return TEST_REPORT();
}
