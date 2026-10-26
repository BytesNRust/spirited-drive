/* port (body shine): the sink's second pass over a registered body texture: on when object_sink_set_shine has tables, only for a texture the
 * class maps know and a polygon that carries normals, and a surface that faces up takes a higher level than one that faces sideways. */
#include "viewer/object_sink.h"
#include "viewer/shine_class.h"
#include "test.h"

#include <string.h>

enum { W = 64, H = 48, TEX = 8, TEXEL = 50 };

static uint8_t screen_px[W * H];
static uint8_t texels[TEX * TEX];
static uint8_t keep[TEX * TEX];
static uint8_t tables[CAR_SHINE_LEVELS][256];
static View view;

static FshEntry texture(void) { return (FshEntry){ { 'b', 'o', 'd', 'y' }, FSH_TYPE_BITMAP, 0, TEX, TEX, { 0 }, texels, sizeof texels }; }

static void setup(void)
{
    view_init(&view, 0, 0, W, H, 90, 90);
    memset(screen_px, 3, sizeof screen_px);
    memset(texels, TEXEL, sizeof texels);
    memset(keep, 0, sizeof keep);   /* all paint */
    for (int l = 0; l < CAR_SHINE_LEVELS; l++)
        for (int i = 0; i < 256; i++) tables[l][i] = (uint8_t)(i == TEXEL ? 100 + l : i);
    shine_class_clear();
    shine_class_add(0, texels, TEX, TEX, keep);
    object_sink_set_shine(NULL, 0);
}

/* a floor 8 below the eye, z 12..24 (seen from steeply enough above to reflect some sky): the camera path's corners */
static const int32_t FLOOR[12] = { -20, -8, 24, 20, -8, 24, 20, -8, 12, -20, -8, 12 };

static int middle(void) { return screen_px[(view.centre_y + 14) * W + view.centre_x]; }

static int draw_floor(const float *normal_of_each_corner)
{
    const FshEntry t = texture();
    const QuadTarget q = { screen_px, W, 0, 0, W, H };
    uint8_t shade[256] = { 0 }, palette[768] = { 0 };
    ObjectSink s = { &q, palette, shade, &view };
    const Object3DSink sink = object_sink(&s);
    Object3DCorners c = { { 0 }, { 0 }, { 0 }, 4, normal_of_each_corner };
    memcpy(c.camera, FLOOR, sizeof FLOOR);
    sink.textured_camera(sink.user, OBJECT3D_TABLE_QUAD_WHOLE, 0, &t, &c);
    return middle();
}

static const float UP[12] = { 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0 };
static const float SIDEWAYS[12] = { 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0 };

static void test_off_by_default_draws_the_texture_only(void)
{
    setup();
    CHECK_EQ_INT(draw_floor(UP), TEXEL);
}

static void test_on_an_up_facing_floor_is_lighter_than_a_sideways_one(void)
{
    setup();
    object_sink_set_shine(tables, 0);
    const int up = draw_floor(UP) - 100;
    setup();
    object_sink_set_shine(tables, 0);
    const int side = draw_floor(SIDEWAYS) - 100;
    CHECK(up >= 0 && side >= 0);        /* both went through a table: 100 + level */
    CHECK(up > CAR_SHINE_NEUTRAL);
    CHECK(side < CAR_SHINE_NEUTRAL);
    object_sink_set_shine(NULL, 0);
}

static void test_no_normals_or_no_class_map_leaves_the_texture(void)
{
    setup();
    object_sink_set_shine(tables, 0);
    CHECK_EQ_INT(draw_floor(NULL), TEXEL);    /* the polygon carries no normals */
    setup();
    object_sink_set_shine(tables, 0);
    shine_class_clear();                      /* a texture the class maps do not know: the wheels, a model */
    CHECK_EQ_INT(draw_floor(UP), TEXEL);
    object_sink_set_shine(NULL, 0);
}

static void test_matte_texels_are_not_touched(void)
{
    setup();
    memset(keep, 255, sizeof keep);
    shine_class_clear();
    shine_class_add(0, texels, TEX, TEX, keep);
    object_sink_set_shine(tables, 0);
    CHECK_EQ_INT(draw_floor(UP), TEXEL);
    object_sink_set_shine(NULL, 0);
}

static void test_the_exe_path_gets_the_pass_too(void)
{
    setup();
    object_sink_set_shine(tables, 0);
    const FshEntry t = texture();
    const QuadTarget q = { screen_px, W, 0, 0, W, H };
    uint8_t shade[256] = { 0 }, palette[768] = { 0 };
    ObjectSink s = { &q, palette, shade, NULL };
    const Object3DSink sink = object_sink(&s);
    CHECK(sink.textured_camera == NULL);
    Object3DCorners c = { { 8, 8, 40, 8, 40, 40, 8, 40 }, { 0, 0, TEX, 0, TEX, TEX, 0, TEX }, { 0 }, 4, UP };
    const int32_t cam[12] = { -20, -8, 24, 20, -8, 24, 20, -8, 12, -20, -8, 12 };
    memcpy(c.camera, cam, sizeof cam);
    sink.textured(sink.user, OBJECT3D_TABLE_QUAD_UV, 0, &t, &c);
    CHECK(screen_px[24 * W + 24] >= 100);   /* the texture's texel went through a level table */
    object_sink_set_shine(NULL, 0);
}

static void test_the_strength_reaches_the_levels(void)
{
    int level[4] = { 0 };
    for (int st = 1; st <= 3; st++) {
        setup();
        object_sink_set_shine(tables, 0);
        object_sink_set_shine_strength(st);
        level[st] = draw_floor(UP) - 100;
    }
    object_sink_set_shine_strength(1);
    object_sink_set_shine(NULL, 0);
    CHECK(level[1] > CAR_SHINE_NEUTRAL);
    CHECK(level[3] > level[1]);   /* the strong look lifts the same panel higher */
}

int main(void)
{
    test_off_by_default_draws_the_texture_only();
    test_on_an_up_facing_floor_is_lighter_than_a_sideways_one();
    test_no_normals_or_no_class_map_leaves_the_texture();
    test_matte_texels_are_not_touched();
    test_the_exe_path_gets_the_pass_too();
    test_the_strength_reaches_the_levels();
    return TEST_REPORT();
}
