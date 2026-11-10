/* port: EXTRAS' Texture Filter on the flat horizon (horizon_smooth.h): the bitmap's texels blended between their
 * centres as it is magnified, each blend the palette's nearest colour; a texel's own colour kept where all four
 * neighbours are it, a 4x4 ordered dither on the blends. Without it the exe's band (horizon.c). */
#include "viewer/horizon_smooth.h"
#include "game/sys/bands.h"
#include "test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

enum { W = 640, H = 480, BW = 640, BH = 8 };

static uint8_t screen[W * H];
static uint8_t texels[BW * BH];
static uint8_t palette[768];

/* colour 0 black, 1 white, 2..33 the greys between them (every 8th level), the rest a far red */
static void make_palette(void)
{
    for (int i = 0; i < 256; i++) palette[3 * i] = 200, palette[3 * i + 1] = 0, palette[3 * i + 2] = 0;
    palette[0] = palette[1] = palette[2] = 0;
    palette[3] = palette[4] = palette[5] = 255;
    for (int g = 0; g < 32; g++) palette[3 * (2 + g)] = palette[3 * (2 + g) + 1] = palette[3 * (2 + g) + 2] = (uint8_t)(g * 8 + 4);
}

static int is_grey_between(uint8_t c) { return c >= 2 && c < 34; }

static const FshEntry *bitmap(void)
{
    static FshEntry e;
    e = (FshEntry){ { 0 }, FSH_TYPE_BITMAP, 0, BW, BH, { 0, 4, 0, 0 }, texels, sizeof texels };
    return &e;
}

static void draw(const HorizonSmooth *s)
{
    static HorizonTables t;
    horizon_tables(&t, W, 90, 0x10000);
    const HorizonTarget target = { screen, W };
    const ClipRect clip = { 0, 0, W, H };
    memset(screen, 0xee, sizeof screen);
    horizon_draw_flat_smooth(s, &t, &target, &clip, bitmap(), 0, 0, 0, 200);
}

static void test_nearest(void)
{
    make_palette();
    static HorizonSmooth s;
    horizon_smooth_palette(&s, palette);
    CHECK_EQ_INT(horizon_smooth_nearest(&s, 20, 20, 20), 2 + 2);       /* grey 20 */
    CHECK_EQ_INT(horizon_smooth_nearest(&s, 130, 130, 130), 2 + 16);   /* grey 132 */
    CHECK_EQ_INT(horizon_smooth_nearest(&s, 200, 0, 0), 34);           /* the first of the reds */
}

/* the top half black, the bottom white: a run of greys where they meet, each row the same across (the dither's
 * 4-pixel pattern) */
static void test_rows_blend(void)
{
    make_palette();
    static HorizonSmooth s;
    horizon_smooth_palette(&s, palette);
    for (int y = 0; y < BH; y++) memset(texels + y * BW, y < BH / 2 ? 0 : 1, BW);
    draw(&s);
    int greys = 0, black = 0, white = 0, last = -1, rising = 1;
    for (int y = 0; y < H; y++) {
        const uint8_t c = screen[y * W + W / 2];
        if (c == 0xee) continue;
        greys += is_grey_between(c), black += c == 0, white += c == 1;
        const int level = c == 0 ? 0 : c == 1 ? 1000 : is_grey_between(c) ? c : -1;
        CHECK(level >= 0);
        if (level < last) rising = 0;
        last = level;
        for (int x = 4; x < W; x++) CHECK(screen[y * W + x] == screen[y * W + x - 4]);   /* the dither's period */
    }
    CHECK(greys >= 2);   /* the exe's band: none */
    CHECK(black > 0 && white > 0);
    CHECK(rising);       /* black, the greys lighter and lighter, white */
}

/* a texel whose neighbours are all it: its own colour, though the palette holds a nearer one at 5 bits */
static void test_uniform_kept(void)
{
    make_palette();
    palette[3 * 40] = 101, palette[3 * 40 + 1] = 102, palette[3 * 40 + 2] = 103;
    palette[3 * 41] = 100, palette[3 * 41 + 1] = 100, palette[3 * 41 + 2] = 100;
    static HorizonSmooth s;
    horizon_smooth_palette(&s, palette);
    memset(texels, 40, sizeof texels);
    draw(&s);
    int other = 0;
    for (int i = 0; i < W * H; i++) other += screen[i] != 40 && screen[i] != 0xee;
    CHECK_EQ_INT(other, 0);
}

/* the panorama wraps: columns left and right of the bitmap read its other end on the same row (the exe reads the
 * row beside it), so a bitmap of one colour per row draws the same pattern across the seam */
static void test_wrap_same_row(void)
{
    make_palette();
    static HorizonSmooth s;
    horizon_smooth_palette(&s, palette);
    for (int y = 0; y < BH; y++) memset(texels + y * BW, 2 + 4 * y, BW);
    static HorizonTables t;
    horizon_tables(&t, W, 90, 0x10000);
    const HorizonTarget target = { screen, W };
    const ClipRect clip = { 0, 0, W, H };
    memset(screen, 0xee, sizeof screen);
    horizon_draw_flat_smooth(&s, &t, &target, &clip, bitmap(), 0x780000, 0, 0, 200);   /* x offset 600: the seam in view */
    for (int y = 0; y < H; y++)
        for (int x = 4; x < W; x++) CHECK(screen[y * W + x] == screen[y * W + x - 4]);
}

/* the PS1 sky: texels index a palette of their own, the screen gets the target palette's nearest colours */
static void test_sky_palettes(void)
{
    make_palette();
    uint8_t decode[768];
    memset(decode, 0, sizeof decode);
    decode[0] = decode[1] = decode[2] = 100;   /* the bitmap's colour 0: grey 100, which the target has as colour 2 + 12 */
    static HorizonSmooth s;
    horizon_smooth_sky(&s, decode, palette);
    memset(texels, 0, sizeof texels);
    CHECK_EQ_INT(horizon_smooth_texel(&s, bitmap(), 5 << 16, 3 << 16, 7, 9), 2 + 12);
    draw(&s);
    CHECK_EQ_INT(screen[200 * W + 100], 2 + 12);   /* a texel's own colour, through the map */
    CHECK_EQ_INT(screen[200 * W + 640 - 1], 2 + 12);
    /* the fills above and under the art are the bitmap's texel 0 and its last, through the same map: not the raw index as a screen entry (a car's red) */
    CHECK_EQ_INT(screen[0 * W + 100], 2 + 12);
    CHECK_EQ_INT(screen[(H - 1) * W + 100], 2 + 12);
}

/* the band cache: any sequence of draws, the inputs changed one at a time (in place too: the same bitmap address with other
 * texels), comes out as the draws without the cache, on any of the bands' slots; and a repeated draw is a copy */
static uint32_t lcg_state = 12345;
static uint32_t lcg(void) { return lcg_state = lcg_state * 1664525u + 1013904223u; }

static void test_cache_exact(void)
{
    static uint8_t reference[W * H], palette_b[768];
    static HorizonSmooth s;
    static HorizonTables t;
    horizon_smooth_cache_reset();
    make_palette();
    memcpy(palette_b, palette, sizeof palette_b);
    for (int i = 0; i < 34; i++) palette_b[3 * i + 2] = (uint8_t)(palette_b[3 * i + 2] + 40);
    for (int i = 0; i < BW * BH; i++) texels[i] = (uint8_t)(lcg() >> 20 & 1 ? 2 + (lcg() >> 24) % 32 : 1);
    horizon_smooth_palette(&s, palette);
    int width = W, fov = 90;
    int32_t scale = 0x20000;
    horizon_tables(&t, width, fov, scale);
    ClipRect clip = { 0, 0, W, H };
    int32_t yaw = 0, line = 205;
    int mirrored = 0, sky_off = 0, band = 0, use_b = 0;
    const unsigned long before = horizon_smooth_cache_hits();
    for (int iter = 0; iter < 600; iter++) {
        switch (lcg() >> 16 & 31) {   /* most iterations repeat; one input moves on the others */
        case 0: texels[lcg() % (BW * BH)] = (uint8_t)(lcg() >> 24); break;
        case 1: use_b ^= 1, horizon_smooth_palette(&s, use_b ? palette_b : palette); break;
        case 2: width = W + (int)(lcg() >> 24) % 3 * 80, horizon_tables(&t, width, fov, scale); break;
        case 3: fov = 60 + (int)(lcg() >> 24) % 3 * 15, scale = 0x18000 + (int32_t)(lcg() >> 24) % 3 * 0x8000, horizon_tables(&t, width, fov, scale); break;
        case 4: clip.x0 = (int)(lcg() >> 24) % 40, clip.x1 = W - (int)(lcg() >> 24) % 40; break;
        case 5: clip.y0 = 190 + (int)(lcg() >> 24) % 30, clip.y1 = 215 + (int)(lcg() >> 24) % 40; break;
        case 6: yaw = (int32_t)(lcg() >> 8) << 4; break;
        case 7: line = 195 + (int32_t)(lcg() >> 24) % 20; break;
        case 8: mirrored ^= 1; break;
        case 9: sky_off ^= 1; break;
        case 10: case 11: band = (int)(lcg() >> 24) % 4; break;
        default: break;
        }
        bands_set_index(band);
        const HorizonTarget target = { screen, W };
        memset(screen, 0xee, sizeof screen);
        horizon_draw_flat_smooth(&s, &t, &target, &clip, bitmap(), yaw, mirrored, sky_off, line);
        horizon_smooth_cache_enable(0);
        const HorizonTarget again = { reference, W };
        memset(reference, 0xee, sizeof reference);
        horizon_draw_flat_smooth(&s, &t, &again, &clip, bitmap(), yaw, mirrored, sky_off, line);
        horizon_smooth_cache_enable(1);
        CHECK(memcmp(screen, reference, sizeof screen) == 0);
    }
    CHECK(horizon_smooth_cache_hits() - before > 150);   /* the repeats were copies, not redraws */
    bands_set_index(0);
    horizon_smooth_cache_reset();
}

/* the band against the scalar blend it replaced (one pixel at a time, the pair of bitmap rows made per column): random
 * bitmaps, palettes up to 255 a channel (the clamps), clips, yaws, scales; both drawn through horizon_flat's own band call */
typedef struct {
    const HorizonSmooth *s;
    const HorizonTables *t;
    const ClipRect *clip;
    uint8_t *pixels;
} RefDraw;

static void ref_fill(void *user, int32_t x, int32_t y, int32_t w, int32_t rows, uint8_t colour)
{
    const RefDraw *d = user;
    (void)x, (void)w;
    if (y < d->clip->y0) rows -= d->clip->y0 - y, y = d->clip->y0;
    if (y + rows > d->clip->y1) rows = d->clip->y1 - y;
    for (int32_t r = 0; r < rows; r++) memset(d->pixels + (size_t)(y + r) * W + d->clip->x0, d->s->map[colour], (size_t)(d->clip->x1 - d->clip->x0));
}

static int32_t ref_wrap(int32_t c, int32_t w)
{
    c %= w;
    return c < 0 ? c + w : c;
}

static void ref_band(void *user, const FshEntry *horz, int32_t x_offset, int32_t first_row, int32_t x, int32_t y)
{
    static const int BAYER_R[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    const RefDraw *d = user;
    const HorizonTables *t = d->t;
    const int32_t w = horz->width, h = horz->height;
    static float at[HORIZON_WIDE_WIDTH + 1];
    int start = 0;   /* where screen column i falls in the bitmap: across a run of columns the table gives one texel */
    for (int i = 1; i <= t->width; i++) {
        if (i < t->width && t->columns[i] == t->columns[start]) continue;
        const int run = i - start;
        for (int k = start; k < i; k++) at[k] = (float)t->columns[start] + ((float)(k - start) + 0.5f) / (float)run;
        start = i;
    }
    const int32_t rows = (int32_t)(((int64_t)t->scale * (h - 1) + 0x8000) >> 16);
    const float step = 65536.0f / (float)t->scale;
    for (int32_t k = 0; k < rows; k++) {
        const int32_t sy = y + k;
        if (sy < d->clip->y0 || sy >= d->clip->y1) continue;
        const float fv = (float)first_row + ((float)k + 0.5f) * step - 0.5f;
        const int32_t r = (int32_t)floorf(fv), wv = (int32_t)((fv - (float)r) * 256);
        const uint8_t *row0 = horz->payload + (size_t)(r < 0 ? 0 : r >= h ? h - 1 : r) * w;
        const uint8_t *row1 = horz->payload + (size_t)(r + 1 < 0 ? 0 : r + 1 >= h ? h - 1 : r + 1) * w;
        for (int i = 0; i < t->width; i++) {
            const int px = x + i;
            if (px < d->clip->x0 || px >= d->clip->x1) continue;
            const float fu = (float)x_offset + at[i] - 0.5f;
            const int32_t c = (int32_t)floorf(fu), cw = (int32_t)((fu - (float)c) * 256);
            const int32_t c0 = ref_wrap(c, w), c1 = ref_wrap(c + 1, w);
            const uint8_t t00 = row0[c0], t10 = row0[c1], t01 = row1[c0], t11 = row1[c1];
            uint8_t out;
            if (t00 == t10 && t00 == t01 && t00 == t11) {
                out = t00;
            } else {
                int rgb[3];
                for (int ch = 0; ch < 3; ch++) {
                    const int32_t top = (256 - cw) * d->s->palette[3 * t00 + ch] + cw * d->s->palette[3 * t10 + ch];
                    const int32_t bottom = (256 - cw) * d->s->palette[3 * t01 + ch] + cw * d->s->palette[3 * t11 + ch];
                    const int32_t dither = (BAYER_R[sy & 3][px & 3] * 2 - 15) * 24 * 256 * 256 / 32 + 256 * 256 / 2;
                    const int32_t m = ((256 - wv) * top + wv * bottom + dither) >> 16;
                    rgb[ch] = m < 0 ? 0 : m > 255 ? 255 : m;
                }
                out = horizon_smooth_nearest(d->s, rgb[0], rgb[1], rgb[2]);
            }
            d->pixels[(size_t)sy * W + px] = out;
        }
    }
}

static void test_band_reference(void)
{
    static uint8_t want[W * H], big[300 * 64];
    static HorizonSmooth s;
    static HorizonTables t;
    horizon_smooth_cache_enable(0);   /* the cache is another test's */
    for (int c = 0; c < 300; c++) {
        for (int i = 0; i < 768; i++) palette[i] = (uint8_t)(lcg() >> 24);
        horizon_smooth_palette(&s, palette);
        const int bw = c % 3 == 0 ? 37 : c % 3 == 1 ? 300 : 211, bh = 6 + (int)(lcg() >> 24) % 50;
        for (int i = 0; i < bw * bh; i++) big[i] = (uint8_t)((lcg() >> 20 & 7) == 0 ? lcg() >> 24 : 40 + (i / bw) / 3 + (i % bw) / 30);
        FshEntry e = { { 0 }, FSH_TYPE_BITMAP, 0, bw, bh, { 0, (int16_t)((lcg() >> 24) % bh), 0, 0 }, big, (size_t)bw * bh };
        horizon_tables(&t, W, 70 + (int)(lcg() >> 24) % 40, 0x10000 + (int32_t)(lcg() >> 24) % 3 * 0x8000);
        const ClipRect clip = { (int)(lcg() >> 24) % 60, (int)(lcg() >> 24) % 60, W - (int)(lcg() >> 24) % 60, H - (int)(lcg() >> 24) % 60 };
        const int32_t yaw = (int32_t)(lcg() >> 8) << 4, line = (int32_t)(lcg() >> 24) % (H + 60) - 30;
        const int mirrored = (int)(lcg() >> 24) & 1, sky_off = (lcg() >> 24) % 5 == 0;
        memset(screen, 0xee, sizeof screen), memset(want, 0xee, sizeof want);
        const HorizonTarget target = { screen, W };
        horizon_draw_flat_smooth(&s, &t, &target, &clip, &e, yaw, mirrored, sky_off, line);
        const RefDraw ref = { &s, &t, &clip, want };
        const HorizonFlatSink sink = { (void *)&ref, ref_fill, ref_band };
        const HorizonBox box = { clip.x0, clip.y0, clip.x1 - clip.x0, clip.y1 - clip.y0 };
        horizon_flat(&e, yaw, mirrored, sky_off, t.scale, &box, line, &sink);
        CHECK(memcmp(screen, want, sizeof screen) == 0);
    }
    horizon_smooth_cache_enable(1);
}

/* the sky's rows (v not changing along a row) come out as the texel at each pixel, as every other row does */
static void test_span_flat_v(void)
{
    make_palette();
    uint8_t decode[768];
    for (int i = 0; i < 256; i++) decode[3 * i] = (uint8_t)(i * 7), decode[3 * i + 1] = (uint8_t)(i * 3), decode[3 * i + 2] = (uint8_t)(255 - i);
    static HorizonSmooth s;
    horizon_smooth_sky(&s, decode, palette);
    lcg_state = 777;
    for (int i = 0; i < BW * BH; i++) texels[i] = (uint8_t)(lcg() >> 24 & 7);   /* few colours: flat quads and blends both */
    static uint8_t row[W];
    for (int n = 0; n < 200; n++) {
        const double u[3] = { (double)(lcg() % (BW * 65536)) - 65536.0 * 3, 3000.0 + (double)(lcg() % 90000) / 7,
                              (n & 1) ? (double)(lcg() % 5000) : 0 };
        const double v[3] = { (double)(lcg() % (BH * 65536)) - 65536.0, 0.0, (double)(lcg() % 3000) / 3 };
        const int x0 = (int)(lcg() % 100), x1 = x0 + 7 + (int)(lcg() % 500), y = (int)(lcg() % 400);
        memset(row, 0xee, sizeof row);
        horizon_smooth_span(&s, bitmap(), row, x0, x1, y, u, v);
        for (int x = x0; x <= x1; x++)
            CHECK_EQ_INT(row[x], horizon_smooth_texel(&s, bitmap(), (int32_t)floor(u[0] + u[1] * x + u[2] * y),
                                                      (int32_t)floor(v[0] + v[1] * x + v[2] * y), x, y));
        CHECK_EQ_INT(row[x1 + 1], 0xee);
    }
}

/* a flat-v span over every kind of u step (backwards, still, slower and faster than a texel a pixel), colours many and few, the bitmap's edges and
 * past them, against the texel at each pixel */
static void test_span_flat_oracle(void)
{
    make_palette();
    uint8_t decode[768];
    for (int i = 0; i < 256; i++) decode[3 * i] = (uint8_t)(i * 7), decode[3 * i + 1] = (uint8_t)(i * 3), decode[3 * i + 2] = (uint8_t)(255 - i);
    static HorizonSmooth s;
    horizon_smooth_sky(&s, decode, palette);
    static uint8_t row[W + 8];
    static const double STEP[] = { 0.0, -3000.0, 20000.0, 65536.0, -65536.0, 70000.0, 200000.0, 1234.5 };
    for (int colours = 0; colours < 3; colours++) {
        lcg_state = 4242u + (uint32_t)colours;
        const uint32_t mask = colours == 0 ? 1u : colours == 1 ? 7u : 255u;
        for (int i = 0; i < BW * BH; i++) texels[i] = (uint8_t)(lcg() >> 24 & mask);
        for (int n = 0; n < 160; n++) {
            const double u[3] = { (double)(lcg() % (BW * 65536 * 2)) - 65536.0 * BW * 0.5, STEP[n & 7], (double)(lcg() % 4000) - 2000.0 };
            const double v[3] = { (double)(lcg() % ((BH + 4) * 65536)) - 65536.0 * 2, 0.0, (double)(lcg() % 3000) / 3 };
            const int x0 = (int)(lcg() % 100), x1 = x0 + (int)(lcg() % 300), y = (int)(lcg() % 400);
            memset(row, 0xee, sizeof row);
            horizon_smooth_span(&s, bitmap(), row, x0, x1, y, u, v);
            for (int x = x0; x <= x1; x++)
                CHECK_EQ_INT(row[x], horizon_smooth_texel(&s, bitmap(), (int32_t)floor(u[0] + u[1] * x + u[2] * y),
                                                          (int32_t)floor(v[0] + v[1] * x + v[2] * y), x, y));
            CHECK_EQ_INT(row[x1 + 1], 0xee);
        }
    }
}

/* TNFS_BENCH_SPAN=1: a sky-like span's cost, ns a pixel (not a check) */
static void bench_span(void)
{
    if (getenv("TNFS_BENCH_SPAN") == NULL) return;
    make_palette();
    uint8_t decode[768];
    for (int i = 0; i < 256; i++) decode[3 * i] = (uint8_t)(i * 7), decode[3 * i + 1] = (uint8_t)(i * 3), decode[3 * i + 2] = (uint8_t)(255 - i);
    static HorizonSmooth s;
    horizon_smooth_sky(&s, decode, palette);
    const char *e = getenv("TNFS_BENCH_STEP");
    const double step = e != NULL ? atof(e) : 22000.0;   /* 16.16 texels a pixel */
    lcg_state = 99;
    for (int i = 0; i < BW * BH; i++) texels[i] = (uint8_t)((i / BW) * 30 + (i % BW) / 40);   /* a sky's gradient: neighbours differ a little */
    static uint8_t row[W];
    struct timespec a, b;
    long px = 0;
    unsigned sum = 0;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (int rep = 0; rep < 400; rep++)
        for (int y = 0; y < 400; y++) {
            const double u[3] = { 1000.0 * rep, step, 0.0 }, v[3] = { 65536.0 * 1.3 + y * 100.0, 0.0, 0.0 };
            horizon_smooth_span(&s, bitmap(), row, 0, W - 1, y, u, v);
            sum += row[rep & 255], px += W;
        }
    clock_gettime(CLOCK_MONOTONIC, &b);
    printf("span bench: %.2f ns/pixel (%ld px, sum %u)\n", ((b.tv_sec - a.tv_sec) * 1e9 + (double)(b.tv_nsec - a.tv_nsec)) / (double)px, px, sum);
}

int main(void)
{
    test_nearest();
    test_rows_blend();
    test_uniform_kept();
    test_wrap_same_row();
    test_sky_palettes();
    test_cache_exact();
    test_band_reference();
    test_span_flat_v();
    test_span_flat_oracle();
    bench_span();
    return TEST_REPORT();
}
