/* port: EXTRAS' Texture Filter and Fog over Modern 3D (modern_road.h's ModernStyle): the filter an ordered 2x2
 * sub-texel offset where a texel spans more than a pixel, the fog a table per depth level towards one colour; with
 * no style the picture as Modern 3D draws it. */
#include "viewer/modern_road.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { W = 640, H = 480, TEX = 16, UNTOUCHED = 3 };

static uint8_t screen[W * H];
static uint8_t texels[TEX * TEX];

static FshEntry texture(void)
{
    return (FshEntry){ { 0 }, FSH_TYPE_BITMAP, 0, TEX, TEX, { 0 }, texels, sizeof texels };
}

/* texels: columns below TEX / 2 colour 10, the rest colour 20 */
static void halves(void)
{
    for (int v = 0; v < TEX; v++)
        for (int u = 0; u < TEX; u++) texels[v * TEX + u] = (uint8_t)(u < TEX / 2 ? 10 : 20);
}

/* a wall facing the eye at depth z, x -w..w, y -w..w, the whole texture over it */
static void wall(const View *view, int32_t z, int32_t w, MapMode mode)
{
    static int32_t c[4][3];
    const int32_t corners[4][3] = { { -w, w, z }, { w, w, z }, { w, -w, z }, { -w, -w, z } };
    memcpy(c, corners, sizeof c);
    const int32_t *const cam[4] = { c[0], c[1], c[2], c[3] };
    const int32_t uv[8] = { 0, 0, TEX << 16, 0, TEX << 16, TEX << 16, 0, TEX << 16 };
    const FshEntry t = texture();
    const QuadTarget q = { screen, W, 0, 0, W, H };
    modern_polygon(&q, view, &t, uv, cam, 4, mode, NULL);
}

/* the columns of the middle row where the two colours meet: 1 without the filter, a few dithered with it */
static int mixed_columns(const View *view)
{
    int changes = 0;
    const uint8_t *row = screen + (size_t)view->centre_y * W;
    for (int x = 1; x < W; x++) changes += row[x] != row[x - 1] && row[x] != UNTOUCHED && row[x - 1] != UNTOUCHED;
    return changes;
}

static void test_filter_magnified(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    halves();
    modern_road_set_style(NULL);
    memset(screen, UNTOUCHED, sizeof screen);
    wall(&view, 100, 40, MAP_OPAQUE);   /* 16 texels over ~256 pixels: 16 pixels a texel */
    CHECK_EQ_INT(mixed_columns(&view), 1);
    const ModernStyle filter = { 1, NULL, 0, 0, 0, 0 };
    modern_road_set_style(&filter);
    memset(screen, UNTOUCHED, sizeof screen);
    wall(&view, 100, 40, MAP_OPAQUE);
    const int mixed = mixed_columns(&view);
    CHECK(mixed > 1 && mixed <= 8);   /* the meeting dithered over under a texel, not the whole wall */
    /* only the two colours: an offset texel, never a new one */
    int other = 0;
    for (int i = 0; i < W * H; i++) other += screen[i] != UNTOUCHED && screen[i] != 10 && screen[i] != 20;
    CHECK_EQ_INT(other, 0);
    modern_road_set_style(NULL);
}

/* a wall far enough that a texel is under a pixel: the filter leaves it as it was */
static void test_filter_minified(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    for (int i = 0; i < TEX * TEX; i++) texels[i] = (uint8_t)(i * 7);
    static uint8_t plain[W * H];
    modern_road_set_style(NULL);
    memset(screen, UNTOUCHED, sizeof screen);
    wall(&view, 4000, 40, MAP_OPAQUE);   /* 16 texels over ~6 pixels */
    memcpy(plain, screen, sizeof plain);
    const ModernStyle filter = { 1, NULL, 0, 0, 0, 0 };
    modern_road_set_style(&filter);
    memset(screen, UNTOUCHED, sizeof screen);
    wall(&view, 4000, 40, MAP_OPAQUE);
    CHECK(memcmp(plain, screen, sizeof plain) == 0);
    modern_road_set_style(NULL);
}

/* the tables: level 0 each colour itself, the last the colour nearest the fog's; 0xfe and 0xff never come out */
static void test_fog_tables(void)
{
    uint8_t palette[768];
    for (int i = 0; i < 256; i++) palette[3 * i] = palette[3 * i + 1] = palette[3 * i + 2] = (uint8_t)(i / 4);
    /* the markers the fog's colour exactly (the greys 1 away), yet barred */
    palette[3 * 0xfe] = 41, palette[3 * 0xfe + 1] = palette[3 * 0xfe + 2] = 40;
    palette[3 * 0xff] = 41, palette[3 * 0xff + 1] = palette[3 * 0xff + 2] = 40;
    static uint8_t fog[MODERN_FOG_LEVELS][256];
    const uint8_t rgb[3] = { 41, 40, 40 };
    modern_fog_tables(palette, rgb, fog);
    for (int c = 0; c < 256; c++) CHECK_EQ_INT(fog[0][c], c);
    CHECK_EQ_INT(fog[MODERN_FOG_LEVELS - 1][0] / 4, 40);   /* grey 40: colours 160..163 */
    CHECK_EQ_INT(fog[MODERN_FOG_LEVELS - 1][255] / 4, 40);
    int barred = 0;
    for (int l = 1; l < MODERN_FOG_LEVELS; l++)
        for (int c = 0; c < 256; c++) barred += fog[l][c] >= 0xfe;
    CHECK_EQ_INT(barred, 0);
    /* half way, black goes half way to the fog */
    const int mid = fog[MODERN_FOG_LEVELS / 2][0] / 4;
    CHECK(mid >= 18 && mid <= 23);
}

/* the light's tables with a cast on the darkened levels: level 0 is untouched (each colour itself), the others lose green and blue to
 * the cast, a grey going olive; with a neutral cast they are the plain tables */
static void test_light_cast(void)
{
    uint8_t palette[768];
    for (int i = 0; i < 256; i++) {
        palette[3 * i] = (uint8_t)((i >> 5) * 36), palette[3 * i + 1] = (uint8_t)(((i >> 2) & 7) * 36), palette[3 * i + 2] = (uint8_t)((i & 3) * 85);
    }
    static uint8_t plain[MODERN_FOG_LEVELS][256], cast[MODERN_FOG_LEVELS][256], neutral[MODERN_FOG_LEVELS][256];
    const float one[3] = { 1, 1, 1 }, olive[3] = { 1.0f, 0.5f, 0.0f }, none[3] = { 1, 1, 1 };
    modern_light_tables(palette, one, plain);
    modern_light_tables_cast(palette, one, olive, cast);
    modern_light_tables_cast(palette, one, none, neutral);
    CHECK(memcmp(plain, neutral, sizeof plain) == 0);
    for (int c = 0; c < 256; c++) CHECK_EQ_INT(cast[0][c], c);
    const int grey = (4 << 5) | (4 << 2) | 1;   /* r 144, g 144, b 85 */
    const int a = plain[4][grey], b = cast[4][grey];
    CHECK(palette[3 * b + 1] < palette[3 * a + 1]);   /* less green than the plain table's */
    CHECK(palette[3 * b + 2] <= palette[3 * a + 2]);
    CHECK(palette[3 * b] >= palette[3 * b + 1]);      /* olive: red leads */
    CHECK_EQ_INT(cast[4][255], 255);
}

/* the tunnel's tables take the tint's brightness, not its hue: at dusk (a red-orange tint) the dim road is as warm as by day, not red;
 * and the cast reaches level 0 once the tint darkens it */
static void test_tunnel_hue(void)
{
    uint8_t palette[768];
    for (int i = 0; i < 256; i++) {
        palette[3 * i] = (uint8_t)((i >> 5) * 36), palette[3 * i + 1] = (uint8_t)(((i >> 2) & 7) * 36), palette[3 * i + 2] = (uint8_t)((i & 3) * 85);
    }
    static uint8_t red[MODERN_FOG_LEVELS][256], warm[MODERN_FOG_LEVELS][256];
    const float dusk[3] = { 0.81f, 0.62f, 0.39f }, cast[3] = { 1.0f, 0.8f, 0.15f };
    modern_light_tables_cast(palette, dusk, cast, red);
    modern_tunnel_tables(palette, dusk, cast, warm);
    const int white = (7 << 5) | (7 << 2) | 3;   /* 252, 252, 255 */
    for (int l = 0; l <= 2; l++) {
        const uint8_t *const r = palette + 3 * red[l][white], *const w = palette + 3 * warm[l][white];
        CHECK(w[1] * r[0] > r[1] * w[0]);   /* more green for its red than the tint's own hue gives */
    }
}

/* a wall before the fog's start untouched; past its end the fog's colour; textured and flat alike */
static void test_fog_depths(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    memset(texels, 10, sizeof texels);
    static uint8_t fog[MODERN_FOG_LEVELS][256];
    for (int l = 0; l < MODERN_FOG_LEVELS; l++)
        for (int c = 0; c < 256; c++) fog[l][c] = (uint8_t)(l == 0 ? c : 100 + l);
    const ModernStyle style = { 0, (const uint8_t (*)[256])fog, 1000, 2000, 0, 0 };
    modern_road_set_style(&style);
    memset(screen, UNTOUCHED, sizeof screen);
    wall(&view, 500, 40, MAP_OPAQUE);
    CHECK_EQ_INT(screen[view.centre_y * W + view.centre_x], 10);
    memset(screen, UNTOUCHED, sizeof screen);
    wall(&view, 3000, 400, MAP_OPAQUE);
    CHECK_EQ_INT(screen[view.centre_y * W + view.centre_x], 100 + MODERN_FOG_LEVELS - 1);
    memset(screen, UNTOUCHED, sizeof screen);
    wall(&view, 1500, 400, MAP_OPAQUE);   /* half way: a middle level */
    const int got = screen[view.centre_y * W + view.centre_x] - 100;
    CHECK(got >= MODERN_FOG_LEVELS / 2 - 2 && got <= MODERN_FOG_LEVELS / 2 + 1);
    static int32_t c[4][3] = { { -400, 400, 3000 }, { 400, 400, 3000 }, { 400, -400, 3000 }, { -400, -400, 3000 } };
    const int32_t *const cam[4] = { c[0], c[1], c[2], c[3] };
    const QuadTarget q = { screen, W, 0, 0, W, H };
    memset(screen, UNTOUCHED, sizeof screen);
    modern_flat(&q, &view, 10, cam, 4);
    CHECK_EQ_INT(screen[view.centre_y * W + view.centre_x], 100 + MODERN_FOG_LEVELS - 1);
    /* a shadow texel (MAP_SHADE 0xfe) shades what is under it, unfogged; a transparent one leaves it */
    memset(texels, 0xff, sizeof texels);
    memset(screen, UNTOUCHED, sizeof screen);
    wall(&view, 3000, 400, MAP_TRANSPARENT);
    CHECK_EQ_INT(screen[view.centre_y * W + view.centre_x], UNTOUCHED);
    modern_road_set_style(NULL);
}

/* the levels in the 4x4 block at the screen's centre, counted */
static void block_levels(const View *view, int counts[MODERN_FOG_LEVELS])
{
    memset(counts, 0, MODERN_FOG_LEVELS * sizeof *counts);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            const int l = screen[((view->centre_y & ~3) + y) * W + (view->centre_x & ~3) + x] - 100;
            if (l >= 0 && l < MODERN_FOG_LEVELS) counts[l]++;
        }
}

/* between two levels the fog is an ordered 4x4 dither of both (no bands); on a level, that level alone */
static void test_fog_dither(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    memset(texels, 10, sizeof texels);
    static uint8_t fog[MODERN_FOG_LEVELS][256];
    for (int l = 0; l < MODERN_FOG_LEVELS; l++)
        for (int c = 0; c < 256; c++) fog[l][c] = (uint8_t)(100 + l);
    const ModernStyle style = { 0, (const uint8_t (*)[256])fog, 1000, 2000, 0, 0 };
    modern_road_set_style(&style);
    int counts[MODERN_FOG_LEVELS];
    memset(screen, UNTOUCHED, sizeof screen);
    wall(&view, 1500, 400, MAP_OPAQUE);   /* level 7.5 */
    block_levels(&view, counts);
    CHECK(counts[7] >= 7 && counts[7] <= 9);
    CHECK_EQ_INT(counts[7] + counts[8], 16);
    memset(screen, UNTOUCHED, sizeof screen);
    wall(&view, 1000 + 1000 * 7 / 15, 400, MAP_OPAQUE);   /* level 7 */
    block_levels(&view, counts);
    CHECK_EQ_INT(counts[7], 16);
    modern_road_set_style(NULL);
}

/* a banked floor (y = x / 2 - 1) out to its vanishing line, a diagonal through the screen's centre, fogged: pixel
 * centres just past its far edge (the edge's growth) have 1 / z at or under 0: as far as can be, fogged whole, not
 * near and clear; the floor's near part (z under the fog's start) its own colour */
static void test_fog_grazing_floor(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    static uint8_t fog[MODERN_FOG_LEVELS][256];
    for (int l = 0; l < MODERN_FOG_LEVELS; l++)
        for (int c = 0; c < 256; c++) fog[l][c] = (uint8_t)(l == 0 ? c : 100 + l);   /* level 0 each colour itself */
    const ModernStyle style = { 0, (const uint8_t (*)[256])fog, 1, 2, 0, 0 };   /* past depth 2 all fog */
    modern_road_set_style(&style);
    static int32_t c[4][3] = { { -4000000, -2000001, 8000000 }, { 4000000, 1999999, 8000000 }, { 40, 19, 10 },
                               { -40, -21, 10 } };
    const int32_t *const cam[4] = { c[0], c[1], c[2], c[3] };
    const QuadTarget q = { screen, W, 0, 0, W, H };
    memset(screen, UNTOUCHED, sizeof screen);
    modern_flat(&q, &view, 10, cam, 4);
    int drawn = 0, odd = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {   /* within 5 rows of the vanishing line (sy = cy - (sx - cx) / 2): depth 64 on */
            const float off = (y + 0.5f) - (view.centre_y - (x + 0.5f - view.centre_x) / 2);
            const uint8_t p = screen[y * W + x];
            if (p != UNTOUCHED && off > -5 && off < 5) drawn++, odd += p != 100 + MODERN_FOG_LEVELS - 1;
        }
    CHECK(drawn > 100);
    CHECK_EQ_INT(odd, 0);
    modern_road_set_style(NULL);
}

/* a road cell, an object's textured polygon and a flat one at the same depth come out with the same fog, pixel for pixel
 * (the cars' fog is the road's: a car is as fogged as the road at its own depth, neither more nor less) */
static void test_fog_same_depth_same_level(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    memset(texels, 10, sizeof texels);
    static uint8_t fog[MODERN_FOG_LEVELS][256];
    for (int l = 0; l < MODERN_FOG_LEVELS; l++)
        for (int c = 0; c < 256; c++) fog[l][c] = (uint8_t)(l == 0 ? c : 100 + l);
    const ModernStyle style = { 0, (const uint8_t (*)[256])fog, 250, 4200, 0, 0 };
    modern_road_set_style(&style);
    static uint8_t road[W * H], car[W * H], flat[W * H];
    static int32_t c[4][3];
    const int32_t uv[8] = { 0, 0, TEX << 16, 0, TEX << 16, TEX << 16, 0, TEX << 16 };
    const FshEntry t = texture();
    int distinct = 0;
    for (int32_t z = 400; z <= 6000; z += 700) {
        /* a floor receding from z to z + 600, as a road cell does */
        const int32_t corners[4][3] = { { -60, -30, z }, { 60, -30, z }, { 60, -30, z + 600 }, { -60, -30, z + 600 } };
        memcpy(c, corners, sizeof c);
        const int32_t *const cam[4] = { c[0], c[1], c[2], c[3] };
        const QuadTarget qr = { road, W, 0, 0, W, H }, qc = { car, W, 0, 0, W, H }, qf = { flat, W, 0, 0, W, H };
        memset(road, UNTOUCHED, sizeof road), memset(car, UNTOUCHED, sizeof car), memset(flat, UNTOUCHED, sizeof flat);
        modern_road_cell(&qr, &view, &t, uv, cam, 0);
        modern_polygon(&qc, &view, &t, uv, cam, 4, MAP_OPAQUE, NULL);
        modern_flat(&qf, &view, 10, cam, 4);
        CHECK(memcmp(road, car, sizeof road) == 0);
        CHECK(memcmp(road, flat, sizeof road) == 0);
        for (int i = 0; i < W * H; i++) distinct += road[i] != UNTOUCHED && road[i] != 10;
    }
    CHECK(distinct > 1000);   /* the fog did touch them */
    modern_road_set_style(NULL);
}

/* the exponential curve: at 2500 the level of the linear curve (about 8 of 15), a trace of shading left at 7000, and never the
 * whole fog (14 of 15 at most) however far */
static void test_fog_exponential(void)
{
    static View view;
    view_init(&view, 0, 0, W, H, 90, 90);
    memset(texels, 10, sizeof texels);
    static uint8_t fog[MODERN_FOG_LEVELS][256];
    for (int l = 0; l < MODERN_FOG_LEVELS; l++)
        for (int c = 0; c < 256; c++) fog[l][c] = (uint8_t)(l == 0 ? c : 100 + l);
    const ModernStyle linear = { 0, (const uint8_t (*)[256])fog, 250, 4200, 0, 0 };
    const ModernStyle expo = { 0, (const uint8_t (*)[256])fog, 250, 4200, 2380, 0.93f };
    const int32_t depths[] = { 400, 1000, 2500, 4200, 7000, 12000, 21840 };
    int near_mean[2] = { 0, 0 }, far_level[2] = { 0, 0 };
    for (int s = 0; s < 2; s++) {
        modern_road_set_style(s == 0 ? &linear : &expo);
        for (size_t i = 0; i < sizeof depths / sizeof *depths; i++) {
            int sum = 0, cells = 0;
            memset(screen, UNTOUCHED, sizeof screen);
            wall(&view, depths[i], depths[i] / 3, MAP_OPAQUE);   /* a big wall: many pixels, the dither averaged */
            for (int y = view.centre_y - 20; y < view.centre_y + 20; y++)
                for (int x = view.centre_x - 20; x < view.centre_x + 20; x++) {
                    const int v = screen[y * W + x];
                    if (v == UNTOUCHED) continue;
                    const int level = v == 10 ? 0 : v - 100;
                    sum += level, cells++;
                    if (s == 1) CHECK(level <= 14);                         /* never the whole fog */
                }
            CHECK(cells > 0);
            const int mean16 = cells ? sum * 16 / cells : 0;                // 16 x the mean level
            if (depths[i] == 2500) near_mean[s] = mean16;
            if (depths[i] == 7000) far_level[s] = mean16;
            if (depths[i] == 21840 && s == 1) CHECK(mean16 > 13 * 16);
        }
    }
    CHECK(abs(near_mean[1] - near_mean[0]) < 2 * 16);   /* the same at 2500, within a level or two */
    CHECK(far_level[0] > 14 * 16);                      /* the plateau: whole at 7000 */
    CHECK(far_level[1] > 11 * 16 && far_level[1] < 14 * 16);   /* the exponential: not yet whole */
    modern_road_set_style(NULL);
}

int main(void)
{
    test_fog_exponential();
    test_fog_grazing_floor();
    test_filter_magnified();
    test_filter_minified();
    test_fog_tables();
    test_light_cast();
    test_tunnel_hue();
    test_fog_depths();
    test_fog_dither();
    test_fog_same_depth_same_level();
    return TEST_REPORT();
}
