/* port (body shine): the object engine's per-vertex camera-space normals (object3d_update_normals): each vertex takes the mean of the
 * faces it belongs to that face the camera, and a sink that asked for them gets them with each polygon's corners. */
#include "game/gfx/object3d.h"
#include "test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { VERTS = 5 };

/* two triangles sharing the edge 0-1: A flat at z 100, B bent toward the camera at the far side */
static int32_t screen[VERTS][2];
static int32_t camera[VERTS][3] = { { -10, 0, 100 }, { 10, 0, 100 }, { 0, -10, 100 }, { 0, 10, 100 }, { 0, 10, 90 } };

static Object3D make(Object3DPolygon *p)
{
    Object3D obj = { 0 };
    obj.vertex_count = VERTS, obj.camera = camera, obj.screen = screen;
    for (int i = 0; i < 2; i++) p[i].flags = POLY_TWO_SIDED | 3, p[i].uv[0] = p[i].uv[1] = p[i].uv[2] = p[i].uv[3] = UINT32_MAX;
    p[0].vertex[0] = 0, p[0].vertex[1] = 1, p[0].vertex[2] = 2;
    p[1].vertex[0] = 0, p[1].vertex[1] = 1, p[1].vertex[2] = 4;
    obj.polygons = p, obj.polygon_count = 2;
    return obj;
}

static float dot(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

static void test_normals_face_the_camera_and_are_unit(void)
{
    Object3DPolygon p[2] = { { 0 } };
    Object3D obj = make(&p[0]);
    CHECK(object3d_enable_normals(&obj));
    object3d_update_normals(&obj);
    const float toward[3] = { 0, 0, -1 };
    for (int v = 0; v < 3; v++) {   /* triangle A alone owns vertex 2: its face normal, toward the eye */
        if (v == 2) {
            CHECK(fabsf(dot(obj.normal[v], obj.normal[v]) - 1.0f) < 1e-4f);
            CHECK(dot(obj.normal[v], toward) > 0.999f);
        }
    }
    free(obj.normal);
}

static void test_a_shared_vertex_takes_the_mean_of_its_faces(void)
{
    Object3DPolygon p[2] = { { 0 } };
    Object3D obj = make(&p[0]);
    CHECK(object3d_enable_normals(&obj));
    object3d_update_normals(&obj);
    const float *a = obj.normal[2], *b = obj.normal[4], *m = obj.normal[0];   /* A's own vertex, B's own, the shared one */
    float sum[3] = { a[0] + b[0], a[1] + b[1], a[2] + b[2] };
    const float len = sqrtf(dot(sum, sum));
    for (int k = 0; k < 3; k++) sum[k] /= len;
    CHECK(fabsf(m[0] - sum[0]) < 1e-4f && fabsf(m[1] - sum[1]) < 1e-4f && fabsf(m[2] - sum[2]) < 1e-4f);
    CHECK(dot(a, b) < 0.999f);   /* the faces really differ */
    free(obj.normal);
}

static void test_a_face_turned_away_adds_nothing(void)
{
    Object3DPolygon p[2] = { { 0 } };
    Object3D obj = make(&p[0]);
    p[1].flags = 3;   /* one-sided: culled by the winding the engine draws by */
    p[0].flags = 3;
    CHECK(object3d_enable_normals(&obj));
    object3d_update_normals(&obj);
    /* of the two triangles at most one faces the eye; a vertex only the culled one owns has no normal */
    const int a_zero = dot(obj.normal[2], obj.normal[2]) == 0.0f, b_zero = dot(obj.normal[4], obj.normal[4]) == 0.0f;
    CHECK(a_zero || b_zero);
    free(obj.normal);
}

static void flat(void *user, uint32_t rgb, const Object3DCorners *c) { (void)user, (void)rgb, (void)c; }

typedef struct {
    int calls;
    int had;
    float n[12];
} Seen;

static void textured(void *user, Object3DTable table, int mode, const FshEntry *texture, const Object3DCorners *c)
{
    (void)table, (void)mode, (void)texture;
    Seen *s = user;
    s->calls++;
    s->had = c->normal != NULL;
    if (c->normal != NULL) memcpy(s->n, c->normal, sizeof s->n);
}

static void test_the_sink_gets_the_corners_normals(void)
{
    Object3DPolygon p[2] = { { 0 } };
    Object3D obj = make(&p[0]);
    FshEntry tex = { { 'b', 'o', 'd', 'y' }, 0, 0, 8, 8, { 0 }, NULL, 0 };
    Object3DTexture slot = { 0, 0, &tex, tex };
    obj.textures = &slot, obj.texture_count = 1;
    p[0].flags = POLY_TWO_SIDED | POLY_TEXTURED | 3;
    p[1].flags = POLY_TWO_SIDED | 3;   /* no texture: not drawn textured */
    uint32_t order[2] = { 0, 1 };
    obj.order = order, obj.order_count = 2;
    Seen seen = { 0 };
    Object3DSink sink = { &seen, textured, flat, NULL, NULL };
    object3d_draw(&obj, 1, &sink);
    CHECK_EQ_INT(seen.calls, 1);
    CHECK(!seen.had);   /* normals off: the exe's calls are as they were */
    CHECK(object3d_enable_normals(&obj));
    object3d_update_normals(&obj);
    memset(&seen, 0, sizeof seen);
    object3d_draw(&obj, 1, &sink);
    CHECK_EQ_INT(seen.calls, 1);
    CHECK(seen.had);
    for (int i = 0; i < 3; i++) CHECK(fabsf(dot(&seen.n[3 * i], &seen.n[3 * i]) - 1.0f) < 1e-3f);
    free(obj.normal);
}

int main(void)
{
    test_normals_face_the_camera_and_are_unit();
    test_a_shared_vertex_takes_the_mean_of_its_faces();
    test_a_face_turned_away_adds_nothing();
    test_the_sink_gets_the_corners_normals();
    return TEST_REPORT();
}
