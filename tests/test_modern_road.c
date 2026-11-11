/* port: EXTRAS' Modern 3D, the near road from its camera-space corners (modern_road.h), and the track drawer handing
 * it the cell instead of 00425718's subdivision (track_draw.h's draw_camera). */
#include "tests/exe_tables_install.h"
#include "game/race/mirror_scene.h"
#include "game/race/track_draw.h"
#include "viewer/modern_road.h"
#include "test.h"

#include <stdlib.h>
#include <math.h>
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

/* port (lamp_pool_tint): a lit road cell with a warmth at each corner takes the warm tables where the interpolated warmth and the dither say. No warmth (NULL, or zero at every corner)
 * is the lit tables alone, to the pixel; the SSE four-pixel path and the scalar tail give the same pixels (a one-column clip is all tail) */
static uint8_t lit_tab[16][256], warm_tab[16][256];

static void draw_lit(const View *view, const QuadTarget *q, const FshEntry *t, const float dark[4], const float *heat)
{
    static int32_t c[4][3];
    const int32_t corners[4][3] = { { -20, -10, 200 }, { 20, -10, 200 }, { 20, -10, 20 }, { -20, -10, 20 } };
    memcpy(c, corners, sizeof c);
    const int32_t *const cam[4] = { c[0], c[1], c[2], c[3] };
    const int32_t uv[8] = { 0, 255 << 16, 255 << 16, 255 << 16, 255 << 16, 0, 0, 0 };
    modern_road_cell_lit_warm(q, view, t, uv, cam, 0, lit_tab, dark, heat != NULL ? warm_tab : NULL, heat, 0);
}

static void test_lit_warm(void)
{
    for (int l = 0; l < 16; l++)
        for (int c = 0; c < 256; c++) lit_tab[l][c] = (uint8_t)((c + l) % 200), warm_tab[l][c] = (uint8_t)((c + l + 100) % 200);
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    const FshEntry t = texture();
    static uint8_t plain[W * H], none[W * H], whole[W * H], warm_all[W * H];
    const float dark[4] = { 2.0f, 9.0f, 9.0f, 2.0f }, zero[4] = { 0, 0, 0, 0 }, one[4] = { 1, 1, 1, 1 }, ramp[4] = { 0.2f, 0.9f, 0.9f, 0.2f };
    QuadTarget q = { screen, W, 0, 0, W, H };
    memset(screen, UNTOUCHED, sizeof screen);
    draw_lit(&view, &q, &t, dark, NULL), memcpy(plain, screen, sizeof plain);
    memset(screen, UNTOUCHED, sizeof screen);
    draw_lit(&view, &q, &t, dark, zero), memcpy(none, screen, sizeof none);
    CHECK(memcmp(plain, none, sizeof plain) == 0);   /* no warmth at any corner: the lit tables alone */
    memset(screen, UNTOUCHED, sizeof screen);
    draw_lit(&view, &q, &t, dark, one), memcpy(warm_all, screen, sizeof warm_all);
    CHECK(memcmp(plain, warm_all, sizeof plain) != 0);   /* full warmth: the warm tables */
    int taken = 0;
    for (int i = 0; i < W * H; i++) taken += plain[i] != UNTOUCHED && warm_all[i] != plain[i];
    CHECK(taken > 1000);
    memset(screen, UNTOUCHED, sizeof screen);
    draw_lit(&view, &q, &t, dark, ramp), memcpy(whole, screen, sizeof whole);
    int lit_px = 0, warm_px = 0;   /* a ramp takes some pixels from each */
    for (int i = 0; i < W * H; i++) {
        if (whole[i] == UNTOUCHED) continue;
        lit_px += whole[i] == plain[i], warm_px += whole[i] == warm_all[i];
    }
    CHECK(lit_px > 1000 && warm_px > 1000);
    /* the four-pixel path against the scalar tail: every column drawn alone (a span of one pixel is all tail) */
    memset(screen, UNTOUCHED, sizeof screen);
    for (int x = 0; x < W; x++) {
        q.clip_left = x, q.clip_right = x + 1;
        draw_lit(&view, &q, &t, dark, ramp);
    }
    CHECK(memcmp(screen, whole, sizeof whole) == 0);
}

/* port (`lamp`, steps): with steps the pool grades in even steps, no dither: a cell of one warmth s / steps is the step s block drawn whole (what the full dither of that block gives), a ramp takes
 * each pixel from the lit tables or one of the blocks and the four-pixel path and the scalar tail agree */
static uint8_t step_tab[3][16][256];

static void draw_steps(const View *view, const QuadTarget *q, const FshEntry *t, const float dark[4], const uint8_t (*warm)[256], const float heat[4], int steps)
{
    static int32_t c[4][3];
    const int32_t corners[4][3] = { { -20, -10, 200 }, { 20, -10, 200 }, { 20, -10, 20 }, { -20, -10, 20 } };
    memcpy(c, corners, sizeof c);
    const int32_t *const cam[4] = { c[0], c[1], c[2], c[3] };
    const int32_t uv[8] = { 0, 255 << 16, 255 << 16, 255 << 16, 255 << 16, 0, 0, 0 };
    modern_road_cell_lit_warm(q, view, t, uv, cam, 0, lit_tab, dark, warm, heat, steps);
}

static void test_lit_steps(void)
{
    for (int l = 0; l < 16; l++)
        for (int c = 0; c < 256; c++) {
            lit_tab[l][c] = (uint8_t)((c + l) % 200);
            for (int s = 0; s < 3; s++) step_tab[s][l][c] = (uint8_t)((c + l + 40 * (s + 1)) % 200);
        }
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    const FshEntry t = texture();
    static uint8_t lit_only[W * H], blocks[3][W * H], ramp_px[W * H], scalar[W * H];
    const float dark[4] = { 2.0f, 9.0f, 9.0f, 2.0f }, ramp[4] = { 0.1f, 0.95f, 0.95f, 0.1f };
    QuadTarget q = { screen, W, 0, 0, W, H };
    memset(screen, UNTOUCHED, sizeof screen);
    draw_lit(&view, &q, &t, dark, NULL), memcpy(lit_only, screen, sizeof lit_only);
    for (int s = 0; s < 3; s++) {   /* a uniform warmth s / 3 is block s whole */
        const float h = (float)(s + 1) / 3.0f, uniform[4] = { h, h, h, h }, full[4] = { 1, 1, 1, 1 };
        memset(screen, UNTOUCHED, sizeof screen);
        draw_steps(&view, &q, &t, dark, step_tab[0], uniform, 3), memcpy(blocks[s], screen, sizeof blocks[s]);
        static uint8_t ref[W * H];
        memset(screen, UNTOUCHED, sizeof screen);
        draw_steps(&view, &q, &t, dark, step_tab[s], full, 0), memcpy(ref, screen, sizeof ref);
        CHECK(memcmp(blocks[s], ref, sizeof ref) == 0);
    }
    memset(screen, UNTOUCHED, sizeof screen);
    draw_steps(&view, &q, &t, dark, step_tab[0], ramp, 3), memcpy(ramp_px, screen, sizeof ramp_px);
    int seen[4] = { 0, 0, 0, 0 }, stray = 0;   /* every pixel is the lit tables' or a block's; all four are in use on the ramp */
    for (int i = 0; i < W * H; i++) {
        if (ramp_px[i] == UNTOUCHED) continue;
        const int is_lit = ramp_px[i] == lit_only[i];
        int hit = is_lit;
        seen[0] += is_lit;
        for (int s = 0; s < 3; s++) seen[s + 1] += ramp_px[i] == blocks[s][i], hit |= ramp_px[i] == blocks[s][i];
        stray += !hit;
    }
    CHECK_EQ_INT(stray, 0);
    CHECK(seen[0] > 100 && seen[1] > 100 && seen[2] > 100 && seen[3] > 100);
    memset(screen, UNTOUCHED, sizeof screen);
    for (int x = 0; x < W; x++) {
        q.clip_left = x, q.clip_right = x + 1;
        draw_steps(&view, &q, &t, dark, step_tab[0], ramp, 3);
    }
    memcpy(scalar, screen, sizeof scalar);
    CHECK(memcmp(scalar, ramp_px, sizeof scalar) == 0);
}

/* a darkness out of the int range (an extrapolated pixel of a polygon that reaches behind the camera: lz * r with r = 1 / IZ_MIN) is level 0 on every path: the four-pixel path (the
 * truncating convert gives the lowest int) and the scalar tail (it was a float to int overflow, undefined; UBSan: "3.38e9 is outside the range of type int") */
static void test_lit_darkness_out_of_range(void)
{
    for (int l = 0; l < 16; l++)
        for (int c = 0; c < 256; c++) lit_tab[l][c] = (uint8_t)((c + l) % 200);
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    const FshEntry t = texture();
    static uint8_t level0[W * H], vector[W * H], tail[W * H];
    const float zero[4] = { 0, 0, 0, 0 }, huge[4] = { 3.38e9f, 3.38e9f, 3.38e9f, 3.38e9f }, nan[4] = { NAN, NAN, NAN, NAN };
    QuadTarget q = { screen, W, 0, 0, W, H };
    memset(screen, UNTOUCHED, sizeof screen);
    draw_lit(&view, &q, &t, zero, NULL), memcpy(level0, screen, sizeof level0);
    memset(screen, UNTOUCHED, sizeof screen);
    draw_lit(&view, &q, &t, huge, NULL), memcpy(vector, screen, sizeof vector);
    CHECK(memcmp(level0, vector, sizeof level0) == 0);
    memset(screen, UNTOUCHED, sizeof screen);
    for (int x = 0; x < W; x++) {   /* every column alone: all scalar tail */
        q.clip_left = x, q.clip_right = x + 1;
        draw_lit(&view, &q, &t, huge, NULL);
    }
    memcpy(tail, screen, sizeof tail);
    CHECK(memcmp(level0, tail, sizeof level0) == 0);
    q.clip_left = 0, q.clip_right = W;
    memset(screen, UNTOUCHED, sizeof screen);
    for (int x = 0; x < W; x++) {
        q.clip_left = x, q.clip_right = x + 1;
        draw_lit(&view, &q, &t, nan, NULL);
    }
    CHECK(memcmp(level0, screen, sizeof level0) == 0);
}

/* port (`lamp`): the tables with a colour added brighten toward it and never darken what the plain tables give; a zero add is the plain tables */
static void test_light_tables_add(void)
{
    uint8_t palette[768];
    for (int i = 0; i < 256; i++) palette[3 * i] = (uint8_t)i, palette[3 * i + 1] = (uint8_t)(i / 2), palette[3 * i + 2] = (uint8_t)(i / 4);
    static uint8_t plain[MODERN_FOG_LEVELS][256], zero[MODERN_FOG_LEVELS][256], red[MODERN_FOG_LEVELS][256];
    const float tint[3] = { 0.6f, 0.6f, 0.7f }, none[3] = { 0, 0, 0 }, add[3] = { 60, 0, 0 };
    modern_light_tables(palette, tint, plain);
    modern_light_tables_add(palette, tint, none, zero);
    modern_light_tables_add(palette, tint, add, red);
    CHECK(memcmp(plain, zero, sizeof plain) == 0);
    int brighter = 0, darker = 0;
    for (int l = 0; l < MODERN_FOG_LEVELS; l++)
        for (int c = 0; c < 0xfe; c++) {
            const int a = plain[l][c], b = red[l][c];
            const int sa = palette[3 * a] + palette[3 * a + 1] + palette[3 * a + 2], sb = palette[3 * b] + palette[3 * b + 1] + palette[3 * b + 2];
            brighter += sb > sa, darker += sb < sa;
        }
    CHECK(brighter > 0 && darker == 0);
    CHECK(red[0][0xfe] == 0xfe && red[0][0xff] == 0xff);   /* the shadow and see-through marks stay */
}

/* i193: a tunnel lamp's warm light, a little of it, on a grey texel: the plain nearest colour is a mauve (its red over green and blue, no yellow: the light's hue turned), the
 * hue-keeping tables take the grey; a warm entry still wins where it is near */
static void test_lamp_tables_keep_the_hue(void)
{
    enum { GREY = 10, MAUVE = 11, BASE = 12, AMBER = 13 };
    uint8_t palette[768];
    memset(palette, 0, sizeof palette);
    const uint8_t grey[3] = { 118, 118, 118 }, mauve[3] = { 136, 112, 118 }, base[3] = { 100, 100, 100 }, amber[3] = { 175, 150, 105 };
    memcpy(palette + 3 * GREY, grey, 3), memcpy(palette + 3 * MAUVE, mauve, 3), memcpy(palette + 3 * BASE, base, 3), memcpy(palette + 3 * AMBER, amber, 3);
    static uint8_t plain[MODERN_FOG_LEVELS][256], hue[MODERN_FOG_LEVELS][256], tunnel[MODERN_FOG_LEVELS][256];
    const float one[3] = { 1, 1, 1 }, warm[3] = { 30, 20, 8 }, strong[3] = { 75, 50, 5 };   /* the base plus warm: (130, 120, 108); plus strong: (175, 150, 105) */
    modern_light_tables_add(palette, one, warm, plain);
    CHECK_EQ_INT(plain[0][BASE], MAUVE);   /* the RGB distance: 200 against the grey's 248 */
    modern_light_tables_add_hue(palette, one, warm, hue);
    CHECK_EQ_INT(hue[0][BASE], GREY);
    modern_tunnel_tables_add_hue(palette, one, one, 0.0f, warm, tunnel);
    CHECK_EQ_INT(tunnel[0][BASE], GREY);
    modern_light_tables_add_hue(palette, one, strong, hue);
    CHECK_EQ_INT(hue[0][BASE], AMBER);   /* a warm entry of the light's own order is taken */
    CHECK(hue[0][0xfe] == 0xfe && hue[0][0xff] == 0xff);
}

int main(void)
{
    exe_tables_for_test();
    test_light_tables_add();
    test_lamp_tables_keep_the_hue();
    test_lit_warm();
    test_lit_steps();
    test_lit_darkness_out_of_range();
    test_t_junction_covered();
    test_perspective_correct();
    test_near_clip_and_box();
    test_transparent();
    test_track_draw_hands_the_cell();
    test_mirror_draw_hands_the_cell();
    return TEST_REPORT();
}
