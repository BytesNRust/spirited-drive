/* port (body shine): the second pass (shine_draw.h) on a software target: only the texels the class map calls paint change, by the level's
 * table; the level runs across the polygon; Modern 3D's pass is perspective-correct; a recorded draw is the direct one. */
#include "game/gfx/raster_record.h"
#include "game/viewer/shine_draw.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { W = 64, H = 48, TEX = 8, START = 50 };

static uint8_t screen_px[W * H];
static uint8_t klass[TEX * TEX];
static uint8_t tables[CAR_SHINE_LEVELS][256];
static const QuadTarget Q = { screen_px, W, 0, 0, W, H };

static void setup(void)
{
    memset(screen_px, START, sizeof screen_px);
    for (int l = 0; l < CAR_SHINE_LEVELS; l++)
        for (int i = 0; i < 256; i++) tables[l][i] = (uint8_t)(i == START ? 100 + l : i);   /* the pixel START shows its level */
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) klass[y * TEX + x] = x < TEX / 2 ? 0 : 255;   /* left half paint, right half matte */
}

static const ShineClass CLS = { klass, TEX, TEX };

static void test_only_paint_texels_change(void)
{
    setup();
    const int32_t screen[8] = { 8, 8, 40, 8, 40, 40, 8, 40 }, uv[8] = { 0, 0, TEX, 0, TEX, TEX, 0, TEX };
    const float level[4] = { 8, 8, 8, 8 };
    shine_draw_affine(&Q, &CLS, tables, screen, uv, level, 4);
    CHECK_EQ_INT(screen_px[24 * W + 14], 108);      /* left half: level 8 */
    CHECK_EQ_INT(screen_px[24 * W + 36], START);    /* right half: matte, untouched */
    CHECK_EQ_INT(screen_px[4 * W + 4], START);      /* outside the polygon */
    CHECK_EQ_INT(screen_px[44 * W + 20], START);
}

static void test_the_level_runs_across_the_polygon(void)
{
    setup();
    for (int i = 0; i < TEX * TEX; i++) klass[i] = 0;   /* all paint */
    const int32_t screen[8] = { 8, 8, 40, 8, 40, 40, 8, 40 }, uv[8] = { 0, 0, TEX, 0, TEX, TEX, 0, TEX };
    const float level[4] = { 0, 15, 15, 0 };            /* dark at the left, light at the right */
    shine_draw_affine(&Q, &CLS, tables, screen, uv, level, 4);
    const int left = screen_px[24 * W + 10] - 100, right = screen_px[24 * W + 37] - 100;
    CHECK(left < 4);
    CHECK(right > 11);
    int last = -1, ok = 1;   /* never darker to the right but for the dither (two levels at most) */
    for (int x = 9; x < 39; x++) {
        const int l = screen_px[24 * W + x] - 100;
        if (l < last - 2) ok = 0;
        last = l > last ? l : last;
    }
    CHECK(ok);
}

static void test_a_triangle_and_the_clip(void)
{
    setup();
    for (int i = 0; i < TEX * TEX; i++) klass[i] = 0;
    const int32_t screen[6] = { 8, 8, 56, 8, 8, 40 }, uv[6] = { 0, 0, TEX, 0, 0, TEX };
    const float level[3] = { 6, 6, 6 };
    QuadTarget clipped = Q;
    clipped.clip_left = 20, clipped.clip_right = 30;
    shine_draw_affine(&clipped, &CLS, tables, screen, uv, level, 3);
    CHECK_EQ_INT(screen_px[12 * W + 25], 106);
    CHECK_EQ_INT(screen_px[12 * W + 15], START);   /* left of the clip */
    CHECK_EQ_INT(screen_px[12 * W + 35], START);   /* right of it */
    CHECK_EQ_INT(screen_px[38 * W + 50], START);   /* outside the triangle */
}

/* a square through pixel centres on its diagonal: every pixel inside is brightened once (the table adds one: twice would show as +2) */
static void test_the_diagonal_is_drawn_once(void)
{
    setup();
    for (int i = 0; i < TEX * TEX; i++) klass[i] = 0;
    for (int l = 0; l < CAR_SHINE_LEVELS; l++)
        for (int i = 0; i < 255; i++) tables[l][i] = (uint8_t)(i + 1);
    const int32_t screen[8] = { 8, 8, 40, 8, 40, 40, 8, 40 }, uv[8] = { 0, 0, TEX, 0, TEX, TEX, 0, TEX };
    const float level[4] = { 5, 5, 5, 5 };
    shine_draw_affine(&Q, &CLS, tables, screen, uv, level, 4);
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
        setup();
        for (int i = 0; i < TEX * TEX; i++) klass[i] = 0;
        for (int l = 0; l < CAR_SHINE_LEVELS; l++)
            for (int i = 0; i < 255; i++) tables[l][i] = (uint8_t)(i + 1);
        const int32_t a[6] = { 8, 8, 40, 8, 8, 40 }, b[6] = { 40, 8, 40, 40, 8, 40 }, bf[6] = { 40, 8, 8, 40, 40, 40 };
        const int32_t uv[6] = { 0, 0, TEX, 0, 0, TEX };
        const float level[3] = { 5, 5, 5 };
        shine_draw_affine(&Q, &CLS, tables, a, uv, level, 3);
        shine_draw_affine(&Q, &CLS, tables, flip ? bf : b, uv, level, 3);
        int missed = 0;
        for (int y = 9; y < 39; y++)
            for (int x = 9; x < 39; x++) missed += screen_px[y * W + x] == START;
        CHECK_EQ_INT(missed, 0);
    }
}

/* the exe's mapper also fills pixels its polygon only touches: a pixel whose centre is within a half pixel outside the outline is reached */
static void test_a_pixel_the_outline_only_touches_is_reached(void)
{
    setup();
    for (int i = 0; i < TEX * TEX; i++) klass[i] = 0;
    const int32_t screen[8] = { 8, 8, 40, 8, 40, 40, 8, 40 }, uv[8] = { 0, 0, TEX, 0, TEX, TEX, 0, TEX };
    const float level[4] = { 8, 8, 8, 8 };
    shine_draw_affine(&Q, &CLS, tables, screen, uv, level, 4);
    CHECK(screen_px[20 * W + 7] != START);    /* the pixel just outside the left edge, its centre half a pixel out */
    CHECK(screen_px[7 * W + 20] != START);    /* and above the top one */
    CHECK_EQ_INT(screen_px[20 * W + 6], START);   /* a whole pixel out is not */
    CHECK_EQ_INT(screen_px[6 * W + 20], START);
}

/* a floor 10 below the eye from z 200 to 20, v 0 .. TEX over it: paint where v < TEX / 2. Perspective-correct, the boundary is at depth 110 */
static void test_camera_pass_is_perspective_correct(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    shine_draw_set_view(&view);
    setup();
    for (int y = 0; y < TEX; y++)
        for (int x = 0; x < TEX; x++) klass[y * TEX + x] = y < TEX / 2 ? 0 : 255;
    const int32_t far_l[3] = { -20, -10, 200 }, far_r[3] = { 20, -10, 200 }, near_r[3] = { 20, -10, 20 }, near_l[3] = { -20, -10, 20 };
    const int32_t *const cam[4] = { far_l, far_r, near_r, near_l };
    const int32_t uv[8] = { 0, 0, TEX << 16, 0, TEX << 16, TEX << 16, 0, TEX << 16 };
    const float level[4] = { 9, 9, 9, 9 };
    shine_draw_camera(&Q, &CLS, tables, uv, cam, level, 4);
    int boundary = -1;
    for (int y = 0; y < H; y++)
        if (screen_px[y * W + view.centre_x] == 109) boundary = y;   /* the last painted row: the nearest of the paint half */
    const int expect = view.centre_y - (int)((double)-10 * view.focal_x / 110);
    CHECK(boundary > 0);
    CHECK(abs(boundary - expect) <= 2);
}

static void test_a_polygon_behind_the_eye_draws_nothing(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    shine_draw_set_view(&view);
    setup();
    for (int i = 0; i < TEX * TEX; i++) klass[i] = 0;
    const int32_t a[3] = { -20, -10, -5 }, b[3] = { 20, -10, -5 }, c[3] = { 0, 10, -50 };
    const int32_t *const cam[4] = { a, b, c, c };
    const int32_t uv[8] = { 0, 0, TEX << 16, 0, 0, TEX << 16, 0, 0 };
    const float level[4] = { 9, 9, 9, 9 };
    shine_draw_camera(&Q, &CLS, tables, uv, cam, level, 3);
    for (int i = 0; i < W * H; i++) CHECK(screen_px[i] == START);
}

static void test_recorded_is_the_direct_draw(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    shine_draw_set_view(&view);
    for (int i = 0; i < TEX * TEX; i++) klass[i] = (uint8_t)((i % 3) == 0 ? 255 : 0);
    const int32_t screen[8] = { 5, 6, 58, 12, 50, 44, 9, 40 }, uv[8] = { 0, 0, TEX, 1, TEX, TEX, 1, TEX };
    const float level[4] = { 2, 12, 9, 5 };
    const int32_t a[3] = { -20, -10, 200 }, b[3] = { 20, -10, 200 }, c[3] = { 20, -10, 20 }, d[3] = { -20, -10, 20 };
    const int32_t *const cam[4] = { a, b, c, d };
    const int32_t uv16[8] = { 0, 0, TEX << 16, 0, TEX << 16, TEX << 16, 0, TEX << 16 };
    static uint8_t direct[W * H];
    setup();
    for (int i = 0; i < W * H; i++) screen_px[i] = (uint8_t)(i * 7 % 3 == 0 ? START : 20);
    uint8_t before[W * H];
    memcpy(before, screen_px, sizeof before);
    shine_draw_affine(&Q, &CLS, tables, screen, uv, level, 4);
    shine_draw_camera(&Q, &CLS, tables, uv16, cam, level, 4);
    memcpy(direct, screen_px, sizeof direct);
    CHECK(memcmp(direct, before, sizeof before) != 0);
    memcpy(screen_px, before, sizeof before);
    struct RasterRecorder *rec = malloc(sizeof *rec);
    raster_record_begin(rec, screen_px);
    shine_draw_affine(&Q, &CLS, tables, screen, uv, level, 4);
    shine_draw_camera(&Q, &CLS, tables, uv16, cam, level, 4);
    CHECK(memcmp(screen_px, before, sizeof before) == 0);   /* nothing drawn until the flush */
    raster_record_end(rec);
    CHECK(memcmp(screen_px, direct, sizeof direct) == 0);
    free(rec);
}

int main(void)
{
    test_only_paint_texels_change();
    test_the_level_runs_across_the_polygon();
    test_a_triangle_and_the_clip();
    test_the_diagonal_is_drawn_once();
    test_a_shared_edge_between_polygons_leaves_no_gap();
    test_a_pixel_the_outline_only_touches_is_reached();
    test_camera_pass_is_perspective_correct();
    test_a_polygon_behind_the_eye_draws_nothing();
    test_recorded_is_the_direct_draw();
    return TEST_REPORT();
}
