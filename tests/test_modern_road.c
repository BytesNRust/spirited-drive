/* port: EXTRAS' Modern 3D, the near road from its camera-space corners (modern_road.h), and the track drawer handing
 * it the cell instead of 00425718's subdivision (track_draw.h's draw_camera). */
#include "game/race/mirror_scene.h"
#include "game/race/track_draw.h"
#include "viewer/modern_road.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { W = 640, H = 480, TEX = 256, UNTOUCHED = 3 };

static uint8_t screen[W * H];
static uint8_t texels[TEX * TEX];

static FshEntry texture(void)
{
    for (int v = 0; v < TEX; v++)
        for (int u = 0; u < TEX; u++) texels[v * TEX + u] = (uint8_t)(v < 0xff ? v : 0xfe);   /* a row's texel: its v */
    return (FshEntry){ { 0 }, FSH_TYPE_BITMAP, 0, TEX, TEX, { 0 }, texels, sizeof texels };
}

/* a floor 10 below the eye, x -20..20, from z `near_z` to 200; v 0 at the near edge to 255 at the far one */
static void floor_quad(const View *view, const QuadTarget *q, const FshEntry *t, int32_t near_z, int transparent)
{
    static int32_t c[4][3];
    const int32_t corners[4][3] = { { -20, -10, 200 }, { 20, -10, 200 }, { 20, -10, near_z }, { -20, -10, near_z } };
    memcpy(c, corners, sizeof c);
    const int32_t *const cam[4] = { c[0], c[1], c[2], c[3] };
    const int32_t uv[8] = { 0, 255 << 16, 255 << 16, 255 << 16, 255 << 16, 0, 0, 0 };
    modern_road_cell(q, view, t, uv, cam, transparent);
}

/* the texel drawn where the floor's middle depth (z 110) lands is the middle of the texture, as the depth says; the
 * affine mapper would take it 0.9 of the way along the screen's span (v ~232) */
static void test_perspective_correct(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    const FshEntry t = texture();
    memset(screen, UNTOUCHED, sizeof screen);
    const QuadTarget q = { screen, W, 0, 0, W, H };
    floor_quad(&view, &q, &t, 20, 0);
    const int y = view.centre_y + (int)(10.0 * view.focal_x / 110.0);
    const int got = screen[y * W + view.centre_x];
    CHECK(abs(got - 127) <= 3);
    /* the near edge's texels at the bottom of the floor, the far edge's at its top */
    const int near_y = view.centre_y + (int)(10.0 * view.focal_x / 20.0) - 1;
    CHECK(screen[near_y * W + view.centre_x] < 8);
    const int far_y = view.centre_y + (int)(10.0 * view.focal_x / 200.0) + 1;
    CHECK(screen[far_y * W + view.centre_x] > 220);
}

/* a floor reaching behind the eye is cut at the near plane, not pushed onto it: drawn down to the screen's bottom
 * (the near plane's line is below it), nothing outside the clip box */
static void test_near_clip_and_box(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    const FshEntry t = texture();
    memset(screen, UNTOUCHED, sizeof screen);
    const QuadTarget q = { screen, W, 100, 300, 540, 430 };   /* its top across the floor */
    floor_quad(&view, &q, &t, -50, 0);
    CHECK(screen[429 * W + W / 2] != UNTOUCHED);
    int outside = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if ((x < 100 || x >= 540 || y < 300 || y >= 430) && screen[y * W + x] != UNTOUCHED) outside++;
    CHECK_EQ_INT(outside, 0);
    /* wholly behind the near plane: nothing */
    memset(screen, UNTOUCHED, sizeof screen);
    static int32_t behind[4][3] = { { -20, -10, 5 }, { 20, -10, 5 }, { 20, -10, -40 }, { -20, -10, -40 } };
    const int32_t *const cam[4] = { behind[0], behind[1], behind[2], behind[3] };
    const int32_t uv[8] = { 0 };
    modern_road_cell(&q, &view, &t, uv, cam, 0);
    int drawn = 0;
    for (int i = 0; i < W * H; i++) drawn += screen[i] != UNTOUCHED;
    CHECK_EQ_INT(drawn, 0);
}

/* a transparent texture's 0xff texels leave the screen as it was */
static void test_transparent(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    FshEntry t = texture();
    memset(texels, 0xff, sizeof texels);
    memset(screen, UNTOUCHED, sizeof screen);
    const QuadTarget q = { screen, W, 0, 0, W, H };
    floor_quad(&view, &q, &t, 20, 1);
    int drawn = 0;
    for (int i = 0; i < W * H; i++) drawn += screen[i] != UNTOUCHED;
    CHECK_EQ_INT(drawn, 0);
    floor_quad(&view, &q, &t, 20, 0);
    CHECK(screen[(view.centre_y + 20) * W + view.centre_x] == 0xff);
}

typedef struct {
    int camera_calls, uv_calls;
    int32_t uv[8];
    const int32_t *cam0;
} Sink;

static void on_uv(void *user, const FshEntry *t, const int32_t uv[8], const int32_t s[8])
{
    (void)t, (void)uv, (void)s;
    ((Sink *)user)->uv_calls++;
}

static void on_camera(void *user, const View *view, const FshEntry *t, const int32_t uv[8], const int32_t *const cam[4])
{
    (void)view, (void)t;
    Sink *s = user;
    s->camera_calls++;
    memcpy(s->uv, uv, sizeof s->uv);
    s->cam0 = cam[0];
}

/* with draw_camera set, a near cell goes to it whole (its uv as 004255ec's at that level), not subdivided into
 * draw_uv; without it the exe's subdivision runs */
static void test_track_draw_hands_the_cell(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    const FshEntry t = texture();
    static int32_t c[4][3] = { { -200, -10, 400 }, { 200, -10, 400 }, { 200, -10, 20 }, { -200, -10, 20 } };
    const int32_t *const cam[4] = { c[0], c[1], c[2], c[3] };
    int32_t s[8];
    for (int i = 0; i < 4; i++) track_project_near(&(DrawContext){ .view = &view }, c[i], 10, &s[2 * i]);
    Sink sink = { 0 };
    DrawContext ctx = { &view, { 0, 0, W, H }, 0, { 0, 0, 0x1999 },
                        { &sink, on_uv, NULL, NULL, NULL, NULL, NULL, on_camera, NULL }, 10, 0 };
    track_draw_subdivided(&ctx, &t, 1, 0, 0, 0, cam, s, 10);
    CHECK_EQ_INT(sink.camera_calls, 1);
    CHECK_EQ_INT(sink.uv_calls, 0);
    CHECK(sink.cam0 == c[0]);
    /* u_level 1, cell 0: the texture's second half (cells count from the far edge), v the whole height */
    CHECK_EQ_INT(sink.uv[0], (TEX / 2) << 16);
    CHECK_EQ_INT(sink.uv[2], (TEX / 2 + TEX / 2 - 1) << 16);
    CHECK_EQ_INT(sink.uv[1], 0);
    CHECK_EQ_INT(sink.uv[5], (TEX - 1) << 16);
    ctx.sink.draw_camera = NULL;
    sink = (Sink){ 0 };
    track_draw_subdivided(&ctx, &t, 1, 0, 0, 0, cam, s, 10);
    CHECK_EQ_INT(sink.camera_calls, 0);
    CHECK(sink.uv_calls > 1);
}

/* the mirror's 00425ed4 the same: a cell whose far corners (1, 2) are behind its near plane, which the exe drops,
 * goes to draw_camera whole (to be clipped there); without it the exe's drop */
static void test_mirror_draw_hands_the_cell(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    const FshEntry t = texture();
    static int32_t c[4][3] = { { -200, -10, 400 }, { -200, -10, 5 }, { 200, -10, 5 }, { 200, -10, 400 } };
    const int32_t *const cam[4] = { c[0], c[1], c[2], c[3] };
    int32_t s[8] = { 0 };   /* the screen points: neither path reads them for this cell */
    Sink sink = { 0 };
    DrawContext ctx = { &view, { 0, 0, W, H }, 0, { 0, 0, 0x1999 },
                        { &sink, on_uv, NULL, NULL, NULL, NULL, NULL, on_camera, NULL }, 10, 0 };
    mirror_draw_subdivided(&ctx, &t, 0, 0, 0, 0, cam, s, 10);
    CHECK_EQ_INT(sink.camera_calls, 1);
    CHECK_EQ_INT(sink.uv_calls, 0);
    CHECK(sink.cam0 == c[0]);
    CHECK_EQ_INT(sink.uv[0], 0);
    CHECK_EQ_INT(sink.uv[2], (TEX - 1) << 16);
    CHECK_EQ_INT(sink.uv[5], (TEX - 1) << 16);
    ctx.sink.draw_camera = NULL;
    sink = (Sink){ 0 };
    mirror_draw_subdivided(&ctx, &t, 0, 0, 0, 0, cam, s, 10);
    CHECK_EQ_INT(sink.camera_calls + sink.uv_calls, 0);   /* the exe: dropped */
}

/* a flat polygon from screen points (tenths of a pixel), at depth 3200 where a tenth of camera x is a tenth of a pixel */
static void flat_at(const View *view, const QuadTarget *q, uint8_t colour, const int32_t tenths[4][2])
{
    static int32_t c[4][3];
    for (int i = 0; i < 4; i++)
        c[i][0] = tenths[i][0] - 10 * view->centre_x, c[i][1] = 10 * view->centre_y - tenths[i][1], c[i][2] = 3200;
    const int32_t *const cam[4] = { c[0], c[1], c[2], c[3] };
    modern_flat(q, view, colour, cam, 4);
}

/* a T junction: one polygon's edge straight from (300, 100) to (300, 300), its neighbours' bent through (300.6, 200)
 * (a road strip of a coarser level beside a finer one). The sliver between them, under a pixel wide, is covered:
 * every polygon reaches EDGE_GROW of a pixel past its outer edges; a lone square still covers its own pixels only */
static void test_t_junction_covered(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    CHECK_EQ_INT(view.focal_x, 320);
    const QuadTarget q = { screen, W, 0, 0, W, H };
    memset(screen, UNTOUCHED, sizeof screen);
    const int32_t left[4][2] = { { 2800, 1000 }, { 3000, 1000 }, { 3000, 3000 }, { 2800, 3000 } };
    const int32_t upper[4][2] = { { 3000, 1000 }, { 3200, 1000 }, { 3200, 2000 }, { 3006, 2000 } };
    const int32_t lower[4][2] = { { 3006, 2000 }, { 3200, 2000 }, { 3200, 3000 }, { 3000, 3000 } };
    flat_at(&view, &q, 1, left);
    flat_at(&view, &q, 2, upper);
    flat_at(&view, &q, 2, lower);
    int holes = 0;
    for (int y = 102; y < 298; y++)
        for (int x = 282; x < 318; x++) holes += screen[y * W + x] == UNTOUCHED;
    CHECK_EQ_INT(holes, 0);
    /* a square on pixel edges, 100..200 across and down: its 100 x 100 pixels, none round it */
    memset(screen, UNTOUCHED, sizeof screen);
    const int32_t square[4][2] = { { 1000, 1000 }, { 2000, 1000 }, { 2000, 2000 }, { 1000, 2000 } };
    flat_at(&view, &q, 1, square);
    int drawn = 0;
    for (int i = 0; i < W * H; i++) drawn += screen[i] != UNTOUCHED;
    CHECK_EQ_INT(drawn, 100 * 100);
}

int main(void)
{
    test_t_junction_covered();
    test_perspective_correct();
    test_near_clip_and_box();
    test_transparent();
    test_track_draw_hands_the_cell();
    test_mirror_draw_hands_the_cell();
    return TEST_REPORT();
}
