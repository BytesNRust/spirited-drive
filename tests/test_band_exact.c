/* Render bands, phase 0: the claim a band split rests on is that a
 * rasterizer clipped to a band of rows paints, inside the band, exactly what the same draw paints unclipped. Every
 * rasterizer the 3D pass uses is drawn over a random background with random primitives (off the clip on every side,
 * degenerate ones too), once with the whole clip box and once as the union of K bands (unequal, random cuts) with the
 * clip's rows narrowed to each; the two pictures must be byte for byte the same. Shade and blend modes read the pixel
 * under the one they write, so the random background is what they read.
 *
 * Known divergences are counted and printed, not asserted: the flat polygons clip their edges geometrically
 * (flat.c:249-250 drops a polygon a band leaves one row), which phase 3's write window removes. */
#include "game/gfx/flat.h"
#include "game/gfx/quad.h"
#include "game/gfx/sprite_scaled.h"
#include "game/race/horizon.h"
#include "game/race/view.h"
#include "viewer/horizon_smooth.h"
#include "viewer/modern_road.h"
#include "viewer/sky_smooth.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { W = 120, H = 90, TW = 16, TH = 12, CASES = 400, MAX_BANDS = 8 };
enum { BOX_L = 3, BOX_T = 2, BOX_R = W - 3, BOX_B = H - 2 };

static uint32_t rng_state = 0x1234567;
static uint32_t rnd(void)
{
    rng_state ^= rng_state << 13, rng_state ^= rng_state >> 17, rng_state ^= rng_state << 5;
    return rng_state;
}
static int32_t span(int32_t lo, int32_t hi) { return lo + (int32_t)(rnd() % (uint32_t)(hi - lo + 1)); }

static uint8_t background[W * H], whole[W * H], banded[W * H];
static uint8_t texels[TW * TH];
static uint8_t shade[256];
static uint8_t shape[0x10 + TW * TH];
static const uint8_t *blend_rows[256];
static uint8_t blend_table[256][256];
static uint8_t palette[768];
static HorizonSmooth smooth;

static FshEntry bitmap(void)
{
    return (FshEntry){ { 0 }, FSH_TYPE_BITMAP, 0, TW, TH, { 0, 4, 0, 0 }, texels, sizeof texels };
}

static void setup(void)
{
    for (int i = 0; i < TW * TH; i++) {
        const uint32_t r = rnd() % 10;
        texels[i] = r == 0 ? 0xff : r == 1 ? 0xfe : (uint8_t)rnd();
    }
    for (int i = 0; i < 256; i++) {
        shade[i] = (uint8_t)(i * 5 + 3);
        for (int j = 0; j < 256; j++) blend_table[i][j] = (uint8_t)(i * 7 + j * 13);
        blend_rows[i] = blend_table[i];
    }
    memset(shape, 0, sizeof shape);
    shape[0] = 0x7b, shape[4] = TW, shape[6] = TH;
    shape[8] = 0, shape[0xa] = 0;
    for (int i = 0; i < TW * TH; i++) shape[0x10 + i] = (uint8_t)(rnd() % 7 == 0 ? 0xff : rnd());
    for (int i = 0; i < 256; i++) palette[3 * i] = (uint8_t)(i * 3), palette[3 * i + 1] = (uint8_t)(i * 5), palette[3 * i + 2] = (uint8_t)(i * 11);
    horizon_smooth_palette(&smooth, palette);
}

static void fresh_background(void)
{
    for (int i = 0; i < W * H; i++) background[i] = (uint8_t)rnd();
}

/* a draw of one primitive, parameters fixed by the case, onto a target */
typedef struct Case {
    int kind;
    int32_t s[10], uv[10], cam[4][3];   /* up to 5 points (the flat polygons of cases 6 and 7) */
    int count, mode, transparent;
    uint32_t colour;
    int32_t w, h, x, y;
    int filter, fog;
} Case;

static const View *the_view(void)
{
    static View view;
    static int made;
    if (!made) view_init(&view, 0, 0, W, H, 90, 90), made = 1;
    return &view;
}

/* top, bottom: the rows a windowed draw may store to (the others ignore them: their clip is the band) */
typedef void (*Draw)(const QuadTarget *q, const Case *c, int top, int bottom);

static void d_quad(const QuadTarget *q, const Case *c, int top, int bottom)
{
    (void)top, (void)bottom;
    const FshEntry t = bitmap();
    quad_draw_mode(q, &t, c->uv, c->s, (MapMode)c->mode, shade);
}
static void d_tri(const QuadTarget *q, const Case *c, int top, int bottom)
{
    (void)top, (void)bottom;
    const FshEntry t = bitmap();
    tri_draw_mode(q, &t, c->uv, c->s, (MapMode)c->mode, shade);
}
static void d_sprite_blend(const QuadTarget *q, const Case *c, int top, int bottom)
{
    (void)top, (void)bottom;
    sprite_scaled_blend(q, blend_rows, shape, c->w, c->h, c->x, c->y);
}
static void d_sprite(const QuadTarget *q, const Case *c, int top, int bottom)
{
    (void)top, (void)bottom;
    sprite_scaled(q, shape, c->w, c->h, c->x, c->y);
}
static void d_sky(const QuadTarget *q, const Case *c, int top, int bottom)
{
    (void)top, (void)bottom;
    const FshEntry t = bitmap();
    sky_smooth_triangle(&smooth, q, &t, c->uv, c->s);
}
static void d_modern(const QuadTarget *q, const Case *c, int top, int bottom)
{
    (void)top, (void)bottom;
    const FshEntry t = bitmap();
    const int32_t *const cam[4] = { c->cam[0], c->cam[1], c->cam[2], c->cam[3] };
    static uint8_t fog[MODERN_FOG_LEVELS][256];
    static int fogged;
    if (!fogged) modern_fog_tables(palette, (const uint8_t[3]){ 200, 100, 50 }, fog), fogged = 1;
    const ModernStyle style = { c->filter, c->fog ? (const uint8_t(*)[256])fog : NULL, 40.0f, 400.0f, 0, 0 };
    modern_road_set_style(c->filter || c->fog ? &style : NULL);
    modern_polygon(q, the_view(), &t, c->uv, cam, c->count, (MapMode)c->mode, shade);
}
static void d_modern_flat(const QuadTarget *q, const Case *c, int top, int bottom)
{
    (void)top, (void)bottom;
    const int32_t *const cam[4] = { c->cam[0], c->cam[1], c->cam[2], c->cam[3] };
    modern_road_set_style(NULL);
    modern_flat(q, the_view(), (uint8_t)c->colour, cam, c->count);
}
static void d_flat(const QuadTarget *q, const Case *c, int top, int bottom)
{
    (void)top, (void)bottom;
    flat_polygon(q, c->colour, c->count, c->s);
}
static void d_flat_shade(const QuadTarget *q, const Case *c, int top, int bottom)
{
    (void)top, (void)bottom;
    flat_polygon_shade(q, shade, c->count, c->s, 0);
}
static void d_fill(const QuadTarget *q, const Case *c, int top, int bottom)
{
    (void)top, (void)bottom;
    flat_fill_rect(q, c->x, c->y, c->w, c->h, c->colour);
}
/* the windowed draws: the whole clip decides the edges, only the band's rows are stored */
static void w_flat(const QuadTarget *q, const Case *c, int top, int bottom)
{
    flat_polygon_rows(q, c->colour, c->count, c->s, top, bottom);
}
static void w_flat_shade(const QuadTarget *q, const Case *c, int top, int bottom)
{
    flat_polygon_shade_rows(q, shade, c->count, c->s, 0, top, bottom);
}
static void w_line(const QuadTarget *q, const Case *c, int top, int bottom)
{
    flat_line_rows(q, c->s[0], c->s[1], c->s[2], c->s[3], c->colour, top, bottom);
}
static void d_line(const QuadTarget *q, const Case *c, int top, int bottom)
{
    (void)top, (void)bottom;
    flat_line(q, c->s[0], c->s[1], c->s[2], c->s[3], c->colour);
}
static void d_horizon_plain(const QuadTarget *q, const Case *c, int top, int bottom)
{
    static HorizonTables tables;
    static int made;
    if (!made) horizon_tables(&tables, W, 90, 0x10000), made = 1;
    const FshEntry t = bitmap();
    const HorizonTarget target = { q->pixels, q->pitch };
    const ClipRect clip = { q->clip_left, q->clip_top, q->clip_right, q->clip_bottom };
    horizon_draw_flat_rows(&tables, &target, &clip, &t, c->x, c->mode, c->y, c->w, top, bottom);
}
static void d_horizon(const QuadTarget *q, const Case *c, int top, int bottom)
{
    (void)top, (void)bottom;
    static HorizonTables tables;
    static int made;
    if (!made) horizon_tables(&tables, W, 90, 0x10000), made = 1;
    const FshEntry t = bitmap();
    const HorizonTarget target = { q->pixels, q->pitch };
    const ClipRect clip = { q->clip_left, q->clip_top, q->clip_right, q->clip_bottom };
    horizon_draw_flat_smooth(&smooth, &tables, &target, &clip, &t, c->x, c->mode, c->y, c->w);
}

/* the points of the primitive of `kind`: convex-ish quads, wild quads, triangles, boxes, on or off the clip box */
static void random_screen(Case *c, int points, int wild)
{
    const int32_t cx = span(-30, W + 30), cy = span(-30, H + 30), r = span(2, 70);
    for (int i = 0; i < points; i++) {
        if (wild) c->s[2 * i] = span(-60, W + 60), c->s[2 * i + 1] = span(-60, H + 60);
        else {
            static const int sx[4] = { -1, 1, 1, -1 }, sy[4] = { -1, -1, 1, 1 };
            c->s[2 * i] = cx + sx[i % 4] * span(1, r), c->s[2 * i + 1] = cy + sy[i % 4] * span(1, r);   /* a 5th point by the first corner */
        }
        c->uv[2 * i] = span(-(4 << 16), (TW + 4) << 16), c->uv[2 * i + 1] = span(-(4 << 16), (TH + 4) << 16);
    }
}

static void random_cam(Case *c, int corners)
{
    const int32_t z = span(30, 500), w = span(5, 150);
    for (int i = 0; i < corners; i++) {
        static const int sx[4] = { -1, 1, 1, -1 }, sy[4] = { 1, 1, -1, -1 };
        c->cam[i][0] = span(-w, w) * 0 + sx[i] * w + span(-w / 4, w / 4) + span(-100, 100);
        c->cam[i][1] = sy[i] * w + span(-w / 4, w / 4) + span(-60, 60);
        c->cam[i][2] = z + span(-20, 80);
        c->uv[2 * i] = (i == 0 || i == 3 ? 0 : TW) << 16, c->uv[2 * i + 1] = (i < 2 ? 0 : TH) << 16;
    }
}

static void random_case(int kind, Case *c)
{
    memset(c, 0, sizeof *c);
    c->kind = kind;
    c->mode = (int)(rnd() % 3);
    c->colour = rnd() & 0xff;
    c->w = span(-40, 60), c->h = span(-40, 60), c->x = span(-40, W + 10), c->y = span(-40, H + 10);
    c->filter = (int)(rnd() & 1), c->fog = (int)(rnd() & 1);
    switch (kind) {
    case 0: random_screen(c, 4, (int)(rnd() % 4 == 0)); break;               /* quad */
    case 1: random_screen(c, 3, 1); for (int i = 0; i < 6; i++) c->uv[i] = span(0, i & 1 ? TH - 1 : TW - 1); break;
    case 2: break;                                                          /* sprites */
    case 3: random_screen(c, 3, 1); for (int i = 0; i < 6; i++) c->uv[i] = span(0, i & 1 ? TH - 1 : TW - 1); break;
    case 4: c->count = 3 + (int)(rnd() & 1); random_cam(c, c->count); break;
    case 5: c->count = 3 + (int)(rnd() & 1); random_cam(c, c->count); break;
    case 6: case 7: random_screen(c, 3 + (int)(rnd() % 3), (int)(rnd() % 3 == 0)); c->count = 3 + (int)(rnd() % 3); break;
    case 8: c->x = span(0, 2000), c->y = 0, c->w = span(20, H), c->mode = (int)(rnd() & 1); break;
    case 9: random_screen(c, 2, 1); break;
    default: c->w = span(0, W), c->x = span(-W, W), c->y = span(0, H), c->mode = (int)(rnd() & 1); break;
    }
}

typedef struct Kind {
    const char *name;
    Draw draw;
    int exact;      /* asserted; the others are counted */
    int cases;
    int windowed;   /* banded with the store window (the whole clip, the band's rows), not a narrowed clip */
} Kind;

static const Kind KINDS[] = {
    { "quad_draw_mode", d_quad, 1, CASES, 0 },
    { "tri_draw_mode", d_tri, 1, CASES, 0 },
    { "sprite_scaled_blend", d_sprite_blend, 1, CASES, 0 },
    { "sprite_scaled", d_sprite, 1, CASES, 0 },
    { "modern_polygon", d_modern, 1, CASES, 0 },
    { "modern_flat", d_modern_flat, 1, CASES, 0 },
    { "flat_polygon (narrowed clip)", d_flat, 0, CASES, 0 },
    { "flat_polygon_shade (narrowed)", d_flat_shade, 0, CASES, 0 },
    { "flat_line (narrowed clip)", d_line, 0, CASES, 0 },
    { "flat_polygon_rows (window)", w_flat, 1, CASES, 1 },
    { "flat_polygon_shade_rows", w_flat_shade, 1, CASES, 1 },
    { "flat_line_rows (window)", w_line, 1, CASES, 1 },
    { "flat_fill_rect", d_fill, 1, CASES, 0 },
    { "horizon_draw_flat_smooth", d_horizon, 1, 60, 0 },
    { "horizon_draw_flat_rows (window)", d_horizon_plain, 1, 60, 1 },
    { "sky_smooth_triangle", d_sky, 1, CASES, 0 },
};
static const int KIND_OF[sizeof KINDS / sizeof KINDS[0]] = { 0, 1, 2, 2, 4, 5, 6, 7, 9, 6, 7, 9, 10, 8, 8, 3 };
/* (random_case's `kind` per row of KINDS) */

static int bands_of(int count, int cuts[MAX_BANDS + 1])
{
    cuts[0] = BOX_T, cuts[count] = BOX_B;
    for (int i = 1; i < count; i++) cuts[i] = span(BOX_T + 1, BOX_B - 1);
    for (int i = 1; i < count; i++)   /* sorted */
        for (int j = i + 1; j < count; j++)
            if (cuts[j] < cuts[i]) { const int t = cuts[i]; cuts[i] = cuts[j]; cuts[j] = t; }
    return count;
}

static int run_kind(const Kind *k, int random_kind, int *painted)
{
    int diffs = 0;
    *painted = 0;
    for (int n = 0; n < k->cases; n++) {
        Case c;
        random_case(random_kind, &c);
        fresh_background();
        memcpy(whole, background, sizeof whole);
        const QuadTarget full = { whole, W, BOX_L, BOX_T, BOX_R, BOX_B };
        k->draw(&full, &c, BOX_T, BOX_B);
        memcpy(banded, background, sizeof banded);
        const QuadTarget full_clip = { banded, W, BOX_L, BOX_T, BOX_R, BOX_B };
        int cuts[MAX_BANDS + 1];
        const int count = 2 + (int)(rnd() % (MAX_BANDS - 1));
        bands_of(count, cuts);
        for (int b = 0; b < count; b++) {
            if (cuts[b] >= cuts[b + 1]) continue;
            const QuadTarget band = { banded, W, BOX_L, cuts[b], BOX_R, cuts[b + 1] };
            if (k->windowed) k->draw(&full_clip, &c, cuts[b], cuts[b + 1]);
            else k->draw(&band, &c, 0, 0);
        }
        if (memcmp(whole, background, sizeof whole) != 0) (*painted)++;   /* the draw did something */
        if (memcmp(whole, banded, sizeof whole) != 0) diffs++;
    }
    return diffs;
}

int main(void)
{
    setup();
    for (size_t i = 0; i < sizeof KINDS / sizeof KINDS[0]; i++) {
        int painted;
        const int diffs = run_kind(&KINDS[i], KIND_OF[i], &painted);
        fprintf(stderr, "band_exact %-26s %3d / %3d cases differ (%d painted)%s\n", KINDS[i].name, diffs, KINDS[i].cases,
                painted, KINDS[i].exact ? "" : " (not asserted)");
        CHECK(painted * 3 > KINDS[i].cases);   /* a test that draws nothing proves nothing */
        if (KINDS[i].exact) CHECK_EQ_INT(diffs, 0);
    }
    return TEST_REPORT();
}
