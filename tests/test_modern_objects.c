/* port: EXTRAS' Modern 3D past the road: the 3D engine's polygons (models and cars) and the scenery boards handed to
 * their sink as camera-space corners, clipped at the near plane and perspective-correct (modern_road.h), with the
 * exe's path untouched when the sink has no camera calls. */
#include "game/gfx/object3d.h"
#include "game/race/track_fence.h"
#include "game/race/track_object_draw.h"
#include "viewer/modern_road.h"
#include "viewer/object_sink.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { W = 640, H = 480, TEX = 64, UNTOUCHED = 3, MAX_POLYS = 4 };

static uint8_t screen_px[W * H];
static uint8_t texels[TEX * TEX];

static FshEntry texture_of(void)
{
    return (FshEntry){ { 0 }, FSH_TYPE_BITMAP, 0, TEX, TEX, { 0 }, texels, sizeof texels };
}

static void fill_rows(void)
{
    for (int v = 0; v < TEX; v++) memset(&texels[v * TEX], v, TEX);   /* a texel's value is its row */
}

/* --- the sink's camera calls --- */

static int32_t first_row_pixel(const View *view, int32_t z, int32_t y_cam)
{
    const int y = view->centre_y - (int)((double)y_cam * view->focal_x / z);
    return screen_px[y * W + view->centre_x];
}

/* a floor 10 below the eye, x -20..20, z 20..200 as the whole texture (OBJ_FULL, opaque) through the sink: the
 * pixel under its middle depth is the middle of the texture, not the affine mapper's 0.9 of the way */
static void test_sink_perspective_correct(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    fill_rows();
    const FshEntry t = texture_of();
    const QuadTarget q = { screen_px, W, 0, 0, W, H };
    uint8_t shade[256] = { 0 }, palette[768] = { 0 };
    ObjectSink s = { &q, palette, shade, &view };
    const Object3DSink sink = object_sink(&s);
    CHECK(sink.textured_camera != NULL && sink.flat_camera != NULL);
    memset(screen_px, UNTOUCHED, sizeof screen_px);
    const Object3DCorners c = { { 0 }, { 0 }, { -20, -10, 200, 20, -10, 200, 20, -10, 20, -20, -10, 20 }, 4 };
    sink.textured_camera(sink.user, OBJECT3D_TABLE_QUAD_WHOLE, 0, &t, &c);
    const int got = first_row_pixel(&view, 110, -10);
    CHECK(abs(got - TEX / 2) <= 2);
    /* the exe's sink has no camera calls */
    s.view = NULL;
    const Object3DSink exe = object_sink(&s);
    CHECK(exe.textured_camera == NULL && exe.flat_camera == NULL);
}

/* mode 2: texel 0xfe takes the shade of the pixel under it; mode 1: 0xff leaves it */
static void test_sink_modes(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    FshEntry t = texture_of();
    memset(texels, 0xfe, sizeof texels);
    const QuadTarget q = { screen_px, W, 0, 0, W, H };
    uint8_t shade[256], palette[768] = { 0 };
    for (int i = 0; i < 256; i++) shade[i] = (uint8_t)(255 - i);
    ObjectSink s = { &q, palette, shade, &view };
    const Object3DSink sink = object_sink(&s);
    const Object3DCorners c = { { 0 }, { 0 }, { -20, -10, 200, 20, -10, 200, 20, -10, 20, -20, -10, 20 }, 4 };
    memset(screen_px, 7, sizeof screen_px);
    sink.textured_camera(sink.user, OBJECT3D_TABLE_QUAD_WHOLE, 2, &t, &c);
    CHECK_EQ_INT(screen_px[(view.centre_y + 30) * W + view.centre_x], 255 - 7);
    memset(texels, 0xff, sizeof texels);
    memset(screen_px, 7, sizeof screen_px);
    sink.textured_camera(sink.user, OBJECT3D_TABLE_QUAD_WHOLE, 1, &t, &c);
    CHECK_EQ_INT(screen_px[(view.centre_y + 30) * W + view.centre_x], 7);
    sink.textured_camera(sink.user, OBJECT3D_TABLE_QUAD_WHOLE, 0, &t, &c);
    CHECK_EQ_INT(screen_px[(view.centre_y + 30) * W + view.centre_x], 0xff);
}

/* the inset shape leaves the texture's border texels out; a mode past the tables draws nothing */
static void test_sink_inset_and_bad_mode(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    FshEntry t = texture_of();
    memset(texels, 2, sizeof texels);
    for (int i = 0; i < TEX; i++) texels[i] = texels[(TEX - 1) * TEX + i] = texels[i * TEX] = texels[i * TEX + TEX - 1] = 1;
    const QuadTarget q = { screen_px, W, 0, 0, W, H };
    uint8_t shade[256] = { 0 }, palette[768] = { 0 };
    ObjectSink s = { &q, palette, shade, &view };
    const Object3DSink sink = object_sink(&s);
    const Object3DCorners c = { { 0 }, { 0 }, { -20, -10, 200, 20, -10, 200, 20, -10, 20, -20, -10, 20 }, 4 };
    memset(screen_px, UNTOUCHED, sizeof screen_px);
    sink.textured_camera(sink.user, OBJECT3D_TABLE_QUAD_WHOLE_TRIANGLES, 0, &t, &c);
    int border = 0, inner = 0;
    for (int i = 0; i < W * H; i++) border += screen_px[i] == 1, inner += screen_px[i] == 2;
    CHECK_EQ_INT(border, 0);
    CHECK(inner > 1000);
    memset(screen_px, UNTOUCHED, sizeof screen_px);
    sink.textured_camera(sink.user, OBJECT3D_TABLE_QUAD_WHOLE_TRIANGLES, 5, &t, &c);   /* slot 14 + 2 = past the five tables */
    int drawn = 0;
    for (int i = 0; i < W * H; i++) drawn += screen_px[i] != UNTOUCHED;
    CHECK_EQ_INT(drawn, 0);
}

/* a flat polygon: the palette's nearest colour, clipped at the near plane */
static void test_sink_flat(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    const QuadTarget q = { screen_px, W, 0, 0, W, H };
    uint8_t shade[256] = { 0 }, palette[768] = { 0 };
    palette[3 * 9] = 200;   /* colour 9: the red; every other colour black */
    ObjectSink s = { &q, palette, shade, &view };
    const Object3DSink sink = object_sink(&s);
    const Object3DCorners c = { { 0 }, { 0 }, { -20, -10, 200, 20, -10, 200, 20, -10, -50, -20, -10, -50 }, 4 };
    memset(screen_px, UNTOUCHED, sizeof screen_px);
    sink.flat_camera(sink.user, 0x000000c8, &c);   /* red in the mesh's packing: the first byte blue, the last red */
    CHECK(screen_px[(H - 1) * W + view.centre_x] != UNTOUCHED);   /* down to the bottom: cut at the near plane, not pushed */
    const Object3DCorners behind = { { 0 }, { 0 }, { -20, -10, -5, 20, -10, -5, 20, -10, -50 }, 3 };
    memset(screen_px, UNTOUCHED, sizeof screen_px);
    sink.flat_camera(sink.user, 0x000000c8, &behind);
    int drawn = 0;
    for (int i = 0; i < W * H; i++) drawn += screen_px[i] != UNTOUCHED;
    CHECK_EQ_INT(drawn, 0);
}

/* --- the engine's calls --- */

typedef struct {
    int textured, camera_textured, flat, camera_flat;
    int32_t camera0[3];
} Calls;

static void on_textured(void *u, Object3DTable t, int m, const FshEntry *tex, const Object3DCorners *c)
{
    (void)t, (void)m, (void)tex, (void)c;
    ((Calls *)u)->textured++;
}
static void on_camera_textured(void *u, Object3DTable t, int m, const FshEntry *tex, const Object3DCorners *c)
{
    (void)t, (void)m, (void)tex;
    Calls *k = u;
    k->camera_textured++;
    memcpy(k->camera0, c->camera, sizeof k->camera0);
}
static void on_flat(void *u, uint32_t rgb, const Object3DCorners *c)
{
    (void)rgb, (void)c;
    ((Calls *)u)->flat++;
}
static void on_camera_flat(void *u, uint32_t rgb, const Object3DCorners *c)
{
    (void)rgb, (void)c;
    ((Calls *)u)->camera_flat++;
}

typedef struct {
    Object3D obj;
    int32_t camera[MAX_POLYS * 4][3], screen[MAX_POLYS * 4][2];
    uint32_t order[MAX_POLYS];
    Object3DPolygon polys[MAX_POLYS];
    Object3DTexture tex;
    FshEntry entry;
} Scene;

/* one quad, textured (slot 0 bound) or flat, its four corners at `corners` in camera space, projected as the engine does */
static void scene_quad(Scene *s, const View *view, uint32_t flags, const int32_t corners[4][3])
{
    memset(s, 0, sizeof *s);
    s->entry = texture_of();
    s->tex.texture = &s->entry;
    s->tex.rgb = 0xc8;
    s->obj.flags = OBJECT3D_FLAG_NEAR;
    s->obj.vertex_count = 4, s->obj.polygon_count = 1, s->obj.order_count = 1;
    s->obj.camera = s->camera, s->obj.screen = s->screen, s->obj.order = s->order, s->obj.polygons = s->polys;
    s->obj.textures = &s->tex, s->obj.texture_count = 1;
    memcpy(s->camera, corners, sizeof(int32_t) * 12);
    for (int i = 0; i < 4; i++) view_project(view, s->camera[i], s->screen[i]);
    s->polys[0] = (Object3DPolygon){ flags | 4, { 0, 1, 2, 3 }, { UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX }, 0 };
}

static const uint32_t TEXTURED_QUAD = POLY_TEXTURED;

/* a polygon reaching behind the near plane: the exe's engine drops it (FLAG_NEAR), the camera sink gets it whole */
static void test_engine_near(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    fill_rows();
    static Scene s;
    const int32_t floor_q[4][3] = { { -20, -10, 200 }, { 20, -10, 200 }, { 20, -10, -50 }, { -20, -10, -50 } };
    scene_quad(&s, &view, TEXTURED_QUAD | POLY_NO_CULL, floor_q);
    Calls k = { 0 };
    const Object3DSink exe = { &k, on_textured, on_flat, NULL, NULL };
    object3d_draw(&s.obj, 30, &exe);
    CHECK_EQ_INT(k.textured, 0);
    const Object3DSink modern = { &k, on_textured, on_flat, on_camera_textured, on_camera_flat };
    object3d_draw(&s.obj, 30, &modern);
    CHECK_EQ_INT(k.textured, 0);
    CHECK_EQ_INT(k.camera_textured, 1);
    CHECK(k.camera0[0] == -20 && k.camera0[1] == -10 && k.camera0[2] == 200);
    /* a flat one (no texture bound to its slot) likewise */
    scene_quad(&s, &view, POLY_NO_CULL, floor_q);
    k = (Calls){ 0 };
    object3d_draw(&s.obj, 30, &exe);
    CHECK_EQ_INT(k.flat, 0);
    object3d_draw(&s.obj, 30, &modern);
    CHECK_EQ_INT(k.camera_flat, 1);
    CHECK_EQ_INT(k.flat, 0);
    /* a line keeps the exe's flat call and its near drop */
    scene_quad(&s, &view, POLY_NO_CULL, floor_q);
    s.polys[0].flags = POLY_NO_CULL | 2;
    s.polys[0].vertex[0] = 2, s.polys[0].vertex[1] = 3;   /* the two behind the eye */
    k = (Calls){ 0 };
    object3d_draw(&s.obj, 30, &modern);
    CHECK_EQ_INT(k.camera_flat + k.flat, 0);
}

/* the camera sink culls by the polygon's facing in camera space as the exe does by its snapped screen winding: the
 * same polygons are drawn, front and reversed, over a spread of triangles in front of the eye */
static uint32_t rng_state = 12345;
static int32_t rnd(int32_t lo, int32_t hi)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return lo + (int32_t)((rng_state >> 8) % (uint32_t)(hi - lo + 1));
}

static void test_engine_facing(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    static Scene s;
    int agreed = 0, drawn = 0;
    for (int n = 0; n < 4000; n++) {
        int32_t c[4][3];
        for (int i = 0; i < 4; i++) c[i][0] = rnd(-900, 900), c[i][1] = rnd(-600, 600), c[i][2] = rnd(300, 4000);
        scene_quad(&s, &view, TEXTURED_QUAD | ((n & 1) ? POLY_REVERSED : 0), c);
        Calls a = { 0 }, b = { 0 };
        const Object3DSink exe = { &a, on_textured, on_flat, NULL, NULL };
        const Object3DSink modern = { &b, on_textured, on_flat, on_camera_textured, on_camera_flat };
        object3d_draw(&s.obj, 30, &exe);
        object3d_draw(&s.obj, 30, &modern);
        /* the exe's winding is of corners snapped to pixels: a sliver edge-on to the eye (twice its area under 100
         * pixels, in a frame of 600) may fall either way, the rest agree */
        const int32_t *p0 = s.screen[0], *p1 = s.screen[1], *p2 = s.screen[2];
        const int32_t area = (p2[1] - p1[1]) * (p0[0] - p1[0]) - (p2[0] - p1[0]) * (p0[1] - p1[1]);
        agreed += (a.textured == b.camera_textured) || abs(area) < 100;
        drawn += a.textured;
    }
    CHECK_EQ_INT(agreed, 4000);
    CHECK(drawn > 1500 && drawn < 2500);   /* about half face the eye, so the test culls both ways */
    /* behind the eye a facing polygon is still told from a back one by the points' triple product: the floor seen
     * from above is drawn, its back (reversed order) is not */
    const int32_t floor_q[4][3] = { { -20, -10, 200 }, { 20, -10, 200 }, { 20, -10, -50 }, { -20, -10, -50 } };
    const int32_t back_q[4][3] = { { -20, -10, -50 }, { 20, -10, -50 }, { 20, -10, 200 }, { -20, -10, 200 } };
    Calls k = { 0 };
    const Object3DSink modern = { &k, on_textured, on_flat, on_camera_textured, on_camera_flat };
    scene_quad(&s, &view, TEXTURED_QUAD, floor_q);
    object3d_draw(&s.obj, 30, &modern);
    const int front = k.camera_textured;
    scene_quad(&s, &view, TEXTURED_QUAD, back_q);
    object3d_draw(&s.obj, 30, &modern);
    CHECK_EQ_INT(front + k.camera_textured, 1);   /* one of the two windings, never both and never neither */
}

/* --- the scenery boards --- */

typedef struct {
    int full, alt, camera;
    const int32_t *cam[4];
} Boards;

static void b_model(void *u, int32_t m, const int32_t p[3], int32_t a) { (void)u, (void)m, (void)p, (void)a; }
static void b_full(void *u, int32_t t, const int32_t s[8]) { (void)t, (void)s, ((Boards *)u)->full++; }
static void b_alt(void *u, int32_t t, const int32_t s[8]) { (void)t, (void)s, ((Boards *)u)->alt++; }
static void b_camera(void *u, int32_t t, const int32_t *const cam[4])
{
    (void)t;
    Boards *b = u;
    b->camera++;
    for (int i = 0; i < 4; i++) b->cam[i] = cam[i];
}

static void test_scenery_boards(void)
{
    uint8_t descriptor[TRACK_WALK_RECORD_SIZE] = { 0 }, record[TRACK_WALK_RECORD_SIZE] = { 0 };
    descriptor[1] = 4;   /* a billboard of four points */
    descriptor[2] = 9;   /* its texture */
    static const int32_t camera[4][3] = { { -100, 50, 400 }, { 100, 50, 400 }, { 100, -50, 400 }, { -100, -50, 400 } };
    static const int32_t screen[4][2] = { { 100, 100 }, { 200, 100 }, { 200, 200 }, { 100, 200 } };
    for (int pass = 0; pass < 2; pass++) {
        TrackObjectWalk walk = { descriptor, record, 1, 0, 1, 4, 4, 0, 0, 4, 0, 0, 0 };
        Boards b = { 0 };
        TrackObjectDraw d = { &walk, NULL, NULL, camera, screen, 4, 0, 0,
                              { &b, b_model, b_full, b_alt, pass ? b_camera : NULL, NULL } };
        track_object_draw(&d, 10, 1);
        CHECK_EQ_INT(b.full, pass ? 0 : 1);
        CHECK_EQ_INT(b.camera, pass ? 1 : 0);
        if (pass) {
            for (int i = 0; i < 4; i++) CHECK(b.cam[i] == camera[i]);   /* the points' own rows, in the screen corners' order */
        }
    }
    /* a two-sided board's faces, the second with its own corner order, both through the camera call */
    uint8_t two[TRACK_WALK_RECORD_SIZE] = { 0 };
    two[1] = 6, two[2] = 3, two[3] = 5;
    static const int32_t cam6[6][3] = { { -100, 50, 400 }, { 100, 50, 400 }, { 100, -50, 400 }, { -100, -50, 400 },
                                        { 0, 50, 600 }, { 0, -50, 600 } };
    static const int32_t scr6[6][2] = { { 100, 100 }, { 200, 100 }, { 200, 200 }, { 100, 200 }, { 150, 100 }, { 150, 200 } };
    TrackObjectWalk walk = { two, record, 1, 0, 1, 6, 6, 0, 0, 6, 0, 0, 0 };
    Boards b = { 0 };
    TrackObjectDraw d = { &walk, NULL, NULL, cam6, scr6, 6, 0, 0, { &b, b_model, b_full, b_alt, b_camera, NULL } };
    track_object_draw(&d, 10, 1);
    CHECK_EQ_INT(b.camera, 2);
    CHECK_EQ_INT(b.alt, 0);
    CHECK(b.cam[0] == cam6[1] && b.cam[1] == cam6[4] && b.cam[2] == cam6[5] && b.cam[3] == cam6[2]);   /* the side: 1, 4, 5, 2 */
}

/* --- the fences --- */

typedef struct {
    int alt, camera, subdivide;
    const int32_t *cam[4];
} Fence;

static FshEntry fence_entry;
static const FshEntry *f_texture(void *c, int32_t flat) { (void)c, (void)flat; return &fence_entry; }
static void f_point(void *c, const int32_t world[3], int32_t camera[3], int32_t screen[2])
{
    (void)c;
    memcpy(camera, world, 3 * sizeof *camera);
    screen[0] = 320 + world[0] / 4, screen[1] = 240 - world[1] / 4;
}
static void f_subdivide(void *c, const FshEntry *t, int32_t l, int32_t u, const int32_t *const cam[4], const int32_t s[8],
                        int32_t n)
{
    (void)t, (void)l, (void)u, (void)cam, (void)s, (void)n;
    ((Fence *)c)->subdivide++;
}
static void f_alt(void *c, const FshEntry *t, const int32_t s[8]) { (void)t, (void)s, ((Fence *)c)->alt++; }
static void f_camera(void *c, const FshEntry *t, const int32_t *const cam[4])
{
    (void)t;
    Fence *f = c;
    f->camera++;
    for (int i = 0; i < 4; i++) f->cam[i] = cam[i];
}

static void test_fences(void)
{
    fence_entry = texture_of();
    static const int32_t world[2][3] = { { -200, 0, 400 }, { 200, 0, 400 } };
    static const int32_t camera[2][3] = { { -200, -50, 400 }, { 200, -50, 400 } };
    static const int32_t screen[2][2] = { { 270, 252 }, { 370, 252 } };
    const TrackFenceInputs in = { world, camera, screen, 30, { 0, 0, 640, 480 }, NULL, NULL };
    const uint8_t pair[2] = { 0, 1 };
    for (int variant = 0; variant < 2; variant++) {
        for (int modern = 0; modern < 2; modern++) {
            Fence f = { 0 };
            const TrackFenceOps ops = { &f, f_texture, f_point, f_subdivide, f_alt, modern ? f_camera : NULL };
            const TrackUv uv = { 0, 0, variant };
            track_fence_draw(&ops, &in, pair, 0, &uv, 0, 0, 1);
            CHECK_EQ_INT(f.subdivide, variant == 0);          /* the subdivided cell goes through track_draw as before */
            CHECK_EQ_INT(f.camera, variant != 0 && modern);
            CHECK_EQ_INT(f.alt, variant != 0 && !modern);
            if (f.camera) {
                CHECK(f.cam[2] == camera[0] && f.cam[3] == camera[1]);   /* a, b: the points' own rows */
                CHECK(f.cam[0][1] > 0 && f.cam[1][1] > 0);               /* the tops, raised by the fence's height */
                CHECK(f.cam[0][0] == 200 && f.cam[1][0] == -200);        /* b's top first, then a's */
            }
        }
    }
}

int main(void)
{
    test_sink_perspective_correct();
    test_sink_modes();
    test_sink_inset_and_bad_mode();
    test_sink_flat();
    test_engine_near();
    test_engine_facing();
    test_scenery_boards();
    test_fences();
    return TEST_REPORT();
}
