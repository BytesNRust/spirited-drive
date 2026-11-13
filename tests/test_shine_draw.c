/* port (body shine): the envmap pass (shine_draw.h) on a software target: only the texels the class map calls paint or shine change, through the
 * table of the pixel's envmap level; the edge rules of the exe's mapper and of Modern 3D's hold for the pass; Modern 3D's pass is
 * perspective-correct; a recorded draw is the direct one. */
#include "game/gfx/raster_record.h"
#include "game/race/car_envmap.h"
#include "game/race/car_repaint.h"
#include "game/viewer/shine_draw.h"
#include "test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { W = 64, H = 48, TEX = 8, START = 50 };

static uint8_t screen_px[W * H];
static uint8_t klass[TEX * TEX];
static const QuadTarget Q = { screen_px, W, 0, 0, W, H };
static const ShineClass CLS = { klass, TEX, TEX };

static void setup(void)
{
    memset(screen_px, START, sizeof screen_px);
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) klass[y * TEX + x] = x < TEX / 2 ? 0 : 255;   /* left half paint, right half matte */
}

/* the envmap image is all sky, and the tables of the envmap are the ones the tests set (env_setup) */
static uint8_t image[CAR_ENVMAP_SIZE * CAR_ENVMAP_SIZE];
static uint8_t paint_tab[CAR_SHINE_LEVELS][256], glass_tab[CAR_SHINE_LEVELS][256], paint_hard[CAR_SHINE_LEVELS][256], glass_hard[CAR_SHINE_LEVELS][256];

/* the envmap pass on the draft's own class codes (car_repaint.h: 0 paint, 1 matte, 2 shine): paint through the paint table, the glass and lamps
 * (shine) through the glass table, matte not at all. The image is all `sample`. */
static EnvShine env_setup(uint8_t sample)
{
    setup();
    memset(image, sample, sizeof image);
    memset(paint_hard, 0, sizeof paint_hard), memset(glass_hard, 0, sizeof glass_hard);
    for (int l = 0; l < CAR_SHINE_LEVELS; l++)
        for (int i = 0; i < 256; i++) {
            paint_tab[l][i] = (uint8_t)(i == START ? 60 + l : i);
            glass_tab[l][i] = (uint8_t)(i == START ? 160 + l : i);
        }
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) klass[y * TEX + x] = x < 3 ? REPAINT_DRAFT_PAINT : x < 6 ? REPAINT_DRAFT_SHINE : REPAINT_DRAFT_MATTE;
    return (EnvShine){ .paint = paint_tab, .glass = glass_tab, .image = image, .paint_hard = paint_hard, .glass_hard = glass_hard };
}

/* the paint table adds one to every entry, so a pixel the pass draws once shows START + 1 and twice START + 2; every texel is paint */
static EnvShine env_plus_one(void)
{
    const EnvShine env = env_setup(255);
    for (int l = 0; l < CAR_SHINE_LEVELS; l++)
        for (int i = 0; i < 255; i++) paint_tab[l][i] = (uint8_t)(i + 1);
    for (int i = 0; i < TEX * TEX; i++) klass[i] = REPAINT_DRAFT_PAINT;
    return env;
}

/* every corner's sphere-map uv at the image's centre */
static void centre(int32_t packed[4])
{
    const float c[2] = { 0.5f, 0.5f };
    for (int i = 0; i < 4; i++) packed[i] = car_envmap_pack(c);
}

static void test_the_envmap_shines_the_drafts_glass(void)
{
    const EnvShine env = env_setup(255);
    const int32_t screen[8] = { 8, 8, 40, 8, 40, 40, 8, 40 }, uv[8] = { 0, 0, TEX, 0, TEX, TEX, 0, TEX };
    int32_t packed[4];
    centre(packed);
    shine_draw_env_affine(&Q, &CLS, &env, screen, uv, packed, 4);
    CHECK(screen_px[24 * W + 10] >= 60 && screen_px[24 * W + 10] < 60 + CAR_SHINE_LEVELS);    /* paint: the paint table */
    CHECK(screen_px[24 * W + 22] >= 160 && screen_px[24 * W + 22] < 160 + CAR_SHINE_LEVELS);  /* shine (glass, lamps): the glass table */
    CHECK_EQ_INT(screen_px[24 * W + 36], START);                                              /* matte: untouched */
}

/* the body class (REPAINT_DRAFT_BODY: a texel that keeps its colour and shines like the body): it goes through the paint table at the body's
 * gain, not through the glass table at the glass's. The paint table is what dims a dark tone, so a black roof shines as the black paint does */
static void test_the_body_class_shines_through_the_paint_table_at_the_body_gain(void)
{
    const EnvShine env = env_setup(255);
    for (int y = 0; y < TEX; y++)
        for (int x = 6; x < TEX; x++) klass[y * TEX + x] = REPAINT_DRAFT_BODY;
    const int32_t screen[8] = { 8, 8, 40, 8, 40, 40, 8, 40 }, uv[8] = { 0, 0, TEX, 0, TEX, TEX, 0, TEX };
    int32_t packed[4];
    centre(packed);
    shine_draw_env_affine(&Q, &CLS, &env, screen, uv, packed, 4);
    const int lo = (int)floorf(car_envmap_level(255, CAR_ENVMAP_BODY_GAIN));
    for (int x = 33; x < 39; x++) {   /* the texels 6 and 7 of the 32-wide polygon, 4 pixels each */
        const int v = screen_px[24 * W + x];
        CHECK(v == 60 + lo || v == 60 + lo + 1);   /* the paint table's entry at the body's level, never the glass table's (160+) */
    }
}

/* the byte whose paint level falls a third of the way from level 8 to 9, whatever the paint's gain */
static uint8_t between_8_and_9(void)
{
    for (int b = 128; b < 256; b++)
        if (car_envmap_level((uint8_t)b, CAR_ENVMAP_BODY_GAIN) >= 8.25f) return (uint8_t)b;
    return 255;
}

/* where the two levels' entries a pixel falls between are far apart in colour (`hard`), the pixel takes the nearer one, not a dither of both */
static void test_a_hard_step_is_not_dithered(void)
{
    const uint8_t sample = between_8_and_9();
    CHECK(car_envmap_level(sample, CAR_ENVMAP_BODY_GAIN) < 8.75f);
    EnvShine env = env_setup(sample);
    const int32_t screen[8] = { 8, 8, 40, 8, 40, 40, 8, 40 }, uv[8] = { 0, 0, 2, 0, 2, TEX, 0, TEX };
    int32_t packed[4];
    centre(packed);
    int lower = 0, upper = 0;
    shine_draw_env_affine(&Q, &CLS, &env, screen, uv, packed, 4);
    for (int y = 10; y < 38; y++)
        for (int x = 10; x < 38; x++) lower += screen_px[y * W + x] == 68, upper += screen_px[y * W + x] == 69;
    CHECK(lower > 0 && upper > 0);   /* an ordinary step is dithered between the two */
    env_setup(sample);
    memset(paint_hard[8], 1, 256);
    shine_draw_env_affine(&Q, &CLS, &env, screen, uv, packed, 4);
    lower = upper = 0;
    for (int y = 10; y < 38; y++)
        for (int x = 10; x < 38; x++) lower += screen_px[y * W + x] == 68, upper += screen_px[y * W + x] == 69;
    CHECK(lower > 0 && upper == 0);  /* a hard one takes the nearer level (8.29: 8) everywhere */
}

/* a dark sample shades the paint below the neutral: the paint table's entries for levels 1 and 2 (61, 62), where a neutral sample gives the
 * neutral level's entry (64) exactly */
static void test_a_dark_sample_shades_the_paint_below_neutral(void)
{
    const float centre_uv[2] = { 0.5f, 0.5f };
    const int32_t c = car_envmap_pack(centre_uv);
    const int32_t packed[4] = { c, c, c, c };
    const int32_t screen[8] = { 8, 8, 40, 8, 40, 40, 8, 40 }, uv[8] = { 0, 0, TEX, 0, TEX, TEX, 0, TEX };
    EnvShine env = env_setup(0);
    shine_draw_env_affine(&Q, &CLS, &env, screen, uv, packed, 4);
    for (int y = 24; y < 26; y++)
        for (int x = 10; x < 14; x++) CHECK(screen_px[y * W + x] >= 61 && screen_px[y * W + x] <= 62);
    env = env_setup(128);
    shine_draw_env_affine(&Q, &CLS, &env, screen, uv, packed, 4);
    CHECK_EQ_INT(screen_px[24 * W + 10], 64);
}

static void test_a_triangle_and_the_clip(void)
{
    const EnvShine env = env_plus_one();
    const int32_t screen[6] = { 8, 8, 56, 8, 8, 40 }, uv[6] = { 0, 0, TEX, 0, 0, TEX };
    int32_t packed[4];
    centre(packed);
    QuadTarget clipped = Q;
    clipped.clip_left = 20, clipped.clip_right = 30;
    shine_draw_env_affine(&clipped, &CLS, &env, screen, uv, packed, 3);
    CHECK_EQ_INT(screen_px[12 * W + 25], START + 1);
    CHECK_EQ_INT(screen_px[12 * W + 15], START);   /* left of the clip */
    CHECK_EQ_INT(screen_px[12 * W + 35], START);   /* right of it */
    CHECK_EQ_INT(screen_px[38 * W + 50], START);   /* outside the triangle */
}

/* a square through pixel centres on its diagonal: every pixel inside is brightened once (the table adds one: twice would show as +2) */
static void test_the_diagonal_is_drawn_once(void)
{
    const EnvShine env = env_plus_one();
    const int32_t screen[8] = { 8, 8, 40, 8, 40, 40, 8, 40 }, uv[8] = { 0, 0, TEX, 0, TEX, TEX, 0, TEX };
    int32_t packed[4];
    centre(packed);
    shine_draw_env_affine(&Q, &CLS, &env, screen, uv, packed, 4);
    int twice = 0, missed = 0;
    for (int y = 9; y < 39; y++)
        for (int x = 9; x < 39; x++) {
            twice += screen_px[y * W + x] == START + 2;
            missed += screen_px[y * W + x] == START;
        }
    CHECK_EQ_INT(twice, 0);
    CHECK_EQ_INT(missed, 0);
}

/* two polygons meeting on a 45 degree edge through pixel centres: the exe's mapper draws those pixels for each, so the pass reaches them
 * for each too (it follows each polygon's own draw, which overwrites the other's) and never leaves a gap, whichever way each winds */
static void test_a_shared_edge_between_polygons_leaves_no_gap(void)
{
    for (int flip = 0; flip < 2; flip++) {
        const EnvShine env = env_plus_one();
        const int32_t a[6] = { 8, 8, 40, 8, 8, 40 }, b[6] = { 40, 8, 40, 40, 8, 40 }, bf[6] = { 40, 8, 8, 40, 40, 40 };
        const int32_t uv[6] = { 0, 0, TEX, 0, 0, TEX };
        int32_t packed[4];
        centre(packed);
        shine_draw_env_affine(&Q, &CLS, &env, a, uv, packed, 3);
        shine_draw_env_affine(&Q, &CLS, &env, flip ? bf : b, uv, packed, 3);
        int missed = 0;
        for (int y = 9; y < 39; y++)
            for (int x = 9; x < 39; x++) missed += screen_px[y * W + x] == START;
        CHECK_EQ_INT(missed, 0);
    }
}

/* the exe's mapper has no pixel-centre offset: a pixel is the polygon's when its (x, y) lies in the closed outline, so a quad from (8,8) to (40,40)
 * fills 8..40 on both axes. The pass keeps to those pixels, no more (a pixel past them was not redrawn by the polygon, and shining it again
 * would brighten it twice: a light line down a seam) and no fewer (a dark dot) */
static void test_the_exe_path_keeps_to_the_mappers_pixels(void)
{
    const EnvShine env = env_plus_one();
    const int32_t screen[8] = { 8, 8, 40, 8, 40, 40, 8, 40 }, uv[8] = { 0, 0, TEX, 0, TEX, TEX, 0, TEX };
    int32_t packed[4];
    centre(packed);
    shine_draw_env_affine(&Q, &CLS, &env, screen, uv, packed, 4);
    CHECK(screen_px[8 * W + 8] != START);     /* the corner pixel */
    CHECK(screen_px[40 * W + 40] != START);   /* the far corner pixel */
    CHECK(screen_px[20 * W + 8] != START && screen_px[20 * W + 40] != START);
    CHECK(screen_px[8 * W + 20] != START && screen_px[40 * W + 20] != START);
    CHECK_EQ_INT(screen_px[20 * W + 7], START);   /* one pixel out is not */
    CHECK_EQ_INT(screen_px[20 * W + 41], START);
    CHECK_EQ_INT(screen_px[7 * W + 20], START);
    CHECK_EQ_INT(screen_px[41 * W + 20], START);
}

/* two quads side by side meeting on x = 24: the mapper draws column 24 for each, the later one over the earlier one's, and the pass follows each
 * draw, so the column is shined once. The test resets the column between the two draws, as the exe's draw does before each polygon's pass */
static void test_the_columns_both_polygons_cover_are_shined_once_each_after_their_own_draw(void)
{
    const EnvShine env = env_plus_one();
    const int32_t left[8] = { 8, 8, 24, 8, 24, 40, 8, 40 }, right[8] = { 24, 8, 40, 8, 40, 40, 24, 40 }, uv[8] = { 0, 0, TEX, 0, TEX, TEX, 0, TEX };
    int32_t packed[4];
    centre(packed);
    shine_draw_env_affine(&Q, &CLS, &env, left, uv, packed, 4);
    for (int y = 8; y <= 40; y++) screen_px[y * W + 24] = START;   /* the right quad's own draw over the shared column */
    shine_draw_env_affine(&Q, &CLS, &env, right, uv, packed, 4);
    for (int y = 9; y < 40; y++) CHECK_EQ_INT(screen_px[y * W + 24], START + 1);
    for (int y = 9; y < 40; y++) CHECK_EQ_INT(screen_px[y * W + 23], START + 1);   /* the left's own, not the right's reach */
}

/* a floor 10 below the eye from z 200 to 20, v 0 .. TEX over it: paint where v < TEX / 2. Perspective-correct, the boundary is at depth 110 */
static void test_camera_pass_is_perspective_correct(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    shine_draw_set_view(&view);
    const EnvShine env = env_plus_one();
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) klass[y * TEX + x] = y < TEX / 2 ? 0 : 255;
    const int32_t far_l[3] = { -20, -10, 200 }, far_r[3] = { 20, -10, 200 }, near_r[3] = { 20, -10, 20 }, near_l[3] = { -20, -10, 20 };
    const int32_t *const cam[4] = { far_l, far_r, near_r, near_l };
    const int32_t uv[8] = { 0, 0, TEX << 16, 0, TEX << 16, TEX << 16, 0, TEX << 16 };
    int32_t packed[4];
    centre(packed);
    shine_draw_env_camera(&Q, &CLS, &env, uv, cam, packed, 4);
    int boundary = -1;
    for (int y = 0; y < H; y++)
        if (screen_px[y * W + view.centre_x] == START + 1) boundary = y;   /* the last painted row: the nearest of the paint half */
    const int expect = view.centre_y - (int)((double)-10 * view.focal_x / 110);
    CHECK(boundary > 0);
    CHECK(abs(boundary - expect) <= 2);
}

static void test_a_polygon_behind_the_eye_draws_nothing(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    shine_draw_set_view(&view);
    const EnvShine env = env_plus_one();
    const int32_t a[3] = { -20, -10, -5 }, b[3] = { 20, -10, -5 }, c[3] = { 0, 10, -50 };
    const int32_t *const cam[4] = { a, b, c, c };
    const int32_t uv[8] = { 0, 0, TEX << 16, 0, 0, TEX << 16, 0, 0 };
    int32_t packed[4];
    centre(packed);
    shine_draw_env_camera(&Q, &CLS, &env, uv, cam, packed, 3);
    for (int i = 0; i < W * H; i++) CHECK(screen_px[i] == START);
}

static void test_recorded_is_the_direct_draw(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    shine_draw_set_view(&view);
    const EnvShine env = env_setup(200);
    for (int i = 0; i < TEX * TEX; i++) klass[i] = (uint8_t)((i % 3) == 0 ? 255 : 0);
    const int32_t screen[8] = { 5, 6, 58, 12, 50, 44, 9, 40 }, uv[8] = { 0, 0, TEX, 1, TEX, TEX, 1, TEX };
    const int32_t a[3] = { -20, -10, 200 }, b[3] = { 20, -10, 200 }, c[3] = { 20, -10, 20 }, d[3] = { -20, -10, 20 };
    const int32_t *const cam[4] = { a, b, c, d };
    const int32_t uv16[8] = { 0, 0, TEX << 16, 0, TEX << 16, TEX << 16, 0, TEX << 16 };
    int32_t packed[4];
    centre(packed);
    static uint8_t direct[W * H];
    for (int i = 0; i < W * H; i++) screen_px[i] = (uint8_t)(i * 7 % 3 == 0 ? START : 20);
    uint8_t before[W * H];
    memcpy(before, screen_px, sizeof before);
    shine_draw_env_affine(&Q, &CLS, &env, screen, uv, packed, 4);
    shine_draw_env_camera(&Q, &CLS, &env, uv16, cam, packed, 4);
    memcpy(direct, screen_px, sizeof direct);
    CHECK(memcmp(direct, before, sizeof before) != 0);
    memcpy(screen_px, before, sizeof before);
    struct RasterRecorder *rec = malloc(sizeof *rec);
    raster_record_begin(rec, screen_px);
    shine_draw_env_affine(&Q, &CLS, &env, screen, uv, packed, 4);
    shine_draw_env_camera(&Q, &CLS, &env, uv16, cam, packed, 4);
    CHECK(memcmp(screen_px, before, sizeof before) == 0);   /* nothing drawn until the flush */
    raster_record_end(rec);
    CHECK(memcmp(screen_px, direct, sizeof direct) == 0);
    free(rec);
}

int main(void)
{
    test_the_envmap_shines_the_drafts_glass();
    test_the_body_class_shines_through_the_paint_table_at_the_body_gain();
    test_a_dark_sample_shades_the_paint_below_neutral();
    test_a_hard_step_is_not_dithered();
    test_a_triangle_and_the_clip();
    test_the_diagonal_is_drawn_once();
    test_a_shared_edge_between_polygons_leaves_no_gap();
    test_the_exe_path_keeps_to_the_mappers_pixels();
    test_the_columns_both_polygons_cover_are_shined_once_each_after_their_own_draw();
    test_camera_pass_is_perspective_correct();
    test_a_polygon_behind_the_eye_draws_nothing();
    test_recorded_is_the_direct_draw();
    return TEST_REPORT();
}
