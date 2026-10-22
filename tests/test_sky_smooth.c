/* port: EXTRAS' Texture Filter on the 3D sky ring (sky_smooth.h): the ring's triangles drawn with the texels blended
 * between their centres as the flat horizon's are (horizon_smooth.h), over the pixels the exe's mapper (tri_draw)
 * would cover, two triangles of a quad meeting without a gap or an overlap. */
#include "viewer/sky_smooth.h"
#include "game/gfx/quad.h"
#include "test.h"

#include <string.h>

enum { W = 320, H = 200, TW = 32, TH = 8, BACK = 0xee };

static uint8_t screen[W * H], other[W * H];
static uint8_t texels[TW * TH];
static uint8_t palette[768];

/* colour 0 black, 1 white, 2..33 the greys between them (every 8th level), the rest a far red */
static void make_palette(void)
{
    for (int i = 0; i < 256; i++) palette[3 * i] = 200, palette[3 * i + 1] = 0, palette[3 * i + 2] = 0;
    palette[0] = palette[1] = palette[2] = 0;
    palette[3] = palette[4] = palette[5] = 255;
    for (int g = 0; g < 32; g++)
        palette[3 * (2 + g)] = palette[3 * (2 + g) + 1] = palette[3 * (2 + g) + 2] = (uint8_t)(g * 8 + 4);
}

static int is_grey_between(uint8_t c) { return c >= 2 && c < 34; }

static const FshEntry *bitmap(void)
{
    static FshEntry e;
    e = (FshEntry){ { 0 }, FSH_TYPE_BITMAP, 0, TW, TH, { 0 }, texels, sizeof texels };
    return &e;
}

static const QuadTarget TARGET = { screen, W, 0, 0, W, H };

/* the quad (corners 0..3 clockwise from the top left) as sky_ring's draw_quad cuts it: (0, 1, 2) and (2, 3, 0) */
static const int32_t QUAD[8] = { 20, 30, 276, 50, 290, 170, 10, 150 };
static const int32_t QUAD_UV[8] = { 1, 1, TW - 2, 1, TW - 2, TH - 2, 1, TH - 2 };

static void halves(int32_t uv_a[6], int32_t s_a[6], int32_t uv_b[6], int32_t s_b[6])
{
    for (int i = 0; i < 6; i++) uv_a[i] = QUAD_UV[i], s_a[i] = QUAD[i];
    for (int i = 0; i < 6; i++) uv_b[i] = QUAD_UV[(4 + i) % 8], s_b[i] = QUAD[(4 + i) % 8];
}

static int count(const uint8_t *pixels, uint8_t colour)
{
    int n = 0;
    for (int i = 0; i < W * H; i++) n += pixels[i] == colour;
    return n;
}

/* a bitmap of one colour comes out as that colour, on the pixels tri_draw covers but for its edges */
static void test_one_colour(void)
{
    make_palette();
    static HorizonSmooth s;
    horizon_smooth_palette(&s, palette);
    memset(texels, 5, sizeof texels);
    int32_t uv_a[6], s_a[6], uv_b[6], s_b[6];
    halves(uv_a, s_a, uv_b, s_b);
    memset(screen, BACK, sizeof screen);
    sky_smooth_triangle(&s, &TARGET, bitmap(), uv_a, s_a);
    sky_smooth_triangle(&s, &TARGET, bitmap(), uv_b, s_b);
    const int smooth = count(screen, 5);
    CHECK_EQ_INT(smooth + count(screen, BACK), W * H);
    memset(screen, BACK, sizeof screen);
    tri_draw(&TARGET, bitmap(), uv_a, s_a, 0);
    tri_draw(&TARGET, bitmap(), uv_b, s_b, 0);
    const int exe = count(screen, 5);
    CHECK(smooth > 30000);
    CHECK(smooth - exe < 300 && exe - smooth < 300);   /* the quad's outline is ~ 700 pixels */
}

/* the quad's two triangles: no pixel twice, none left inside */
static void test_halves_meet(void)
{
    make_palette();
    static HorizonSmooth s;
    horizon_smooth_palette(&s, palette);
    memset(texels, 5, sizeof texels);
    int32_t uv_a[6], s_a[6], uv_b[6], s_b[6];
    halves(uv_a, s_a, uv_b, s_b);
    memset(screen, BACK, sizeof screen);
    memset(other, BACK, sizeof other);
    const QuadTarget second = { other, W, 0, 0, W, H };
    sky_smooth_triangle(&s, &TARGET, bitmap(), uv_a, s_a);
    sky_smooth_triangle(&s, &second, bitmap(), uv_b, s_b);
    int both = 0, gaps = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int a = screen[y * W + x] != BACK, b = other[y * W + x] != BACK;
            both += a && b;
            /* on the diagonal from corner 0 (20, 30) to corner 2 (290, 170), well inside the quad */
            if (x > 40 && x < 270 && y * 270 == (x - 20) * 140 + 30 * 270) gaps += !a && !b;
        }
    CHECK_EQ_INT(both, 0);
    CHECK_EQ_INT(gaps, 0);
}

/* left half of the bitmap black, right half white: black at the left, white at the right, greys where they meet
 * (the exe's mapper: only black and white) */
static void test_blend(void)
{
    make_palette();
    static HorizonSmooth s;
    horizon_smooth_palette(&s, palette);
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) texels[y * TW + x] = x < TW / 2 ? 0 : 1;
    int32_t uv_a[6], s_a[6], uv_b[6], s_b[6];
    halves(uv_a, s_a, uv_b, s_b);
    memset(screen, BACK, sizeof screen);
    sky_smooth_triangle(&s, &TARGET, bitmap(), uv_a, s_a);
    sky_smooth_triangle(&s, &TARGET, bitmap(), uv_b, s_b);
    const int y = 100;
    CHECK_EQ_INT(screen[y * W + 30], 0);
    CHECK_EQ_INT(screen[y * W + 270], 1);
    int greys = 0;
    for (int x = 0; x < W; x++) greys += is_grey_between(screen[y * W + x]);
    CHECK(greys >= 4);
    memset(screen, BACK, sizeof screen);
    tri_draw(&TARGET, bitmap(), uv_a, s_a, 0);
    tri_draw(&TARGET, bitmap(), uv_b, s_b, 0);
    greys = 0;
    for (int x = 0; x < W; x++) greys += is_grey_between(screen[y * W + x]);
    CHECK_EQ_INT(greys, 0);
}

/* nothing outside the clip box */
static void test_clip(void)
{
    make_palette();
    static HorizonSmooth s;
    horizon_smooth_palette(&s, palette);
    memset(texels, 5, sizeof texels);
    int32_t uv_a[6], s_a[6], uv_b[6], s_b[6];
    halves(uv_a, s_a, uv_b, s_b);
    memset(screen, BACK, sizeof screen);
    const QuadTarget box = { screen, W, 100, 60, 200, 120 };
    sky_smooth_triangle(&s, &box, bitmap(), uv_a, s_a);
    sky_smooth_triangle(&s, &box, bitmap(), uv_b, s_b);
    int outside = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (x < 100 || x >= 200 || y < 60 || y >= 120) outside += screen[y * W + x] != BACK;
    CHECK_EQ_INT(outside, 0);
    CHECK_EQ_INT(count(screen, 5), 100 * 60);
}

/* a triangle far off the screen, or with all corners on a line, draws nothing */
static void test_degenerate(void)
{
    make_palette();
    static HorizonSmooth s;
    horizon_smooth_palette(&s, palette);
    memset(texels, 5, sizeof texels);
    memset(screen, BACK, sizeof screen);
    const int32_t uv[6] = { 1, 1, 30, 1, 30, 6 };
    const int32_t line[6] = { 10, 10, 100, 100, 200, 200 };
    const int32_t far[6] = { -50000, -50000, -40000, -50000, -40000, -40000 };
    sky_smooth_triangle(&s, &TARGET, bitmap(), uv, line);
    sky_smooth_triangle(&s, &TARGET, bitmap(), uv, far);
    CHECK_EQ_INT(count(screen, BACK), W * H);
}

int main(void)
{
    test_one_colour();
    test_halves_meet();
    test_blend();
    test_clip();
    test_degenerate();
    return TEST_REPORT();
}
