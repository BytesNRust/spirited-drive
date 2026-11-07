/* port: the flat horizon blended (horizon_smooth.h). */
#include "viewer/horizon_smooth.h"

#include "game/sys/bands.h"
#include "game/sys/palette.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { CHOICES = 0xfe };   /* 0xfe and 0xff mark shadow and see-through texels */

/* the table for `palette` (what the screen can show) */
static void make_nearest(HorizonSmooth *s, const uint8_t palette[768])
{
    const uint8_t *usable = palette_world_usable();   /* not the entries the race rewrites (the brake lamp, the police lights) */
    for (int cell = 0; cell < HORIZON_SMOOTH_CELLS; cell++) {
        /* the cell's middle */
        const int r = (cell >> 10 & 31) * 8 + 4, g = (cell >> 5 & 31) * 8 + 4, b = (cell & 31) * 8 + 4;
        int best = 0, best_d = 1 << 30;
        for (int i = 0; i < CHOICES; i++) {
            if (!usable[i]) continue;
            const int dr = palette[3 * i] - r, dg = palette[3 * i + 1] - g, db = palette[3 * i + 2] - b;
            const int d = dr * dr + dg * dg + db * db;
            if (d < best_d) best = i, best_d = d;
        }
        s->nearest[cell] = (uint8_t)best;
    }
}

void horizon_smooth_palette(HorizonSmooth *s, const uint8_t palette[768])
{
    if (s->ready && memcmp(s->palette, palette, sizeof s->palette) == 0) return;
    memcpy(s->palette, palette, sizeof s->palette);
    make_nearest(s, palette);
    for (int i = 0; i < 256; i++) s->map[i] = (uint8_t)i;
    s->ready = 1;
}

void horizon_smooth_sky(HorizonSmooth *s, const uint8_t decode[768], const uint8_t target[768])
{
    if (s->ready && memcmp(s->palette, decode, sizeof s->palette) == 0 && memcmp(s->target, target, sizeof s->target) == 0) return;
    memcpy(s->palette, decode, sizeof s->palette);
    memcpy(s->target, target, sizeof s->target);
    make_nearest(s, target);
    for (int i = 0; i < 256; i++) s->map[i] = horizon_smooth_nearest(s, decode[3 * i], decode[3 * i + 1], decode[3 * i + 2]);
    s->ready = 1;
}

uint8_t horizon_smooth_nearest(const HorizonSmooth *s, int r, int g, int b)
{
    return s->nearest[(r >> 3) << 10 | (g >> 3) << 5 | b >> 3];
}

typedef struct {
    const HorizonSmooth *s;
    const HorizonTables *t;
    const HorizonTarget *target;
    const ClipRect *clip;
} Draw;

/* 004829b0 over the clip's width, clipped to its rows (as horizon.c's); the colour through the map as every texel is (the PS1 sky's texels index a palette of their own:
 * the raw index was a screen entry of the cars' fit, a red block under the art) */
static void smooth_fill(void *user, int32_t x, int32_t y, int32_t w, int32_t rows, uint8_t colour)
{
    const Draw *d = user;
    (void)x, (void)w;
    if (y < d->clip->y0) rows -= d->clip->y0 - y, y = d->clip->y0;
    if (y + rows > d->clip->y1) rows = d->clip->y1 - y;
    for (int32_t r = 0; r < rows; r++)
        memset(d->target->pixels + (size_t)(y + r) * d->target->pitch + d->clip->x0, d->s->map[colour],
               (size_t)(d->clip->x1 - d->clip->x0));
}

/* where screen column i falls in the bitmap (its texel's left edge at the column the tables give): across the run of
 * columns that table gives the same texel */
static void column_positions(const HorizonTables *t, float *at)
{
    int start = 0;
    for (int i = 1; i <= t->width; i++) {
        if (i < t->width && t->columns[i] == t->columns[start]) continue;
        const int run = i - start;
        for (int k = start; k < i; k++) at[k] = (float)t->columns[start] + ((float)(k - start) + 0.5f) / (float)run;
        start = i;
    }
}

/* the 4x4 ordered dither, BAYER * 2 - 15 by (y & 3, x & 3), times DITHER / 32: a blend between two palette colours
 * comes out as a pattern of both, not the nearer alone */
static const int8_t BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
enum { DITHER = 24, ONE = 256 };   /* ONE: the blend weights' scale */

/* a screen column's two bitmap columns (wrapped on the row) and the right one's weight */
typedef struct {
    int32_t c0, c1, w;
} Column;

static int32_t wrap(int32_t c, int32_t w)
{
    c %= w;
    return c < 0 ? c + w : c;
}

/* a screen column over a pair of bitmap rows: the two rows blended across (RGB times ONE), or the one texel all four
 * are (-1 none) */
typedef struct {
    int32_t top[4], bottom[4];   /* the fourth lane 0: the blend runs the three channels in a register's lanes */
    int16_t only;
} Pair;

static void pair_make(const HorizonSmooth *s, const Column *c, const uint8_t *row0, const uint8_t *row1, Pair *p)
{
    const uint8_t t00 = row0[c->c0], t10 = row0[c->c1], t01 = row1[c->c0], t11 = row1[c->c1];
    p->only = t00 == t10 && t00 == t01 && t00 == t11 ? s->map[t00] : -1;
    if (p->only >= 0) return;   /* one texel: no blend to make, its index is the pixel */
    for (int k = 0; k < 3; k++) {
        p->top[k] = (ONE - c->w) * s->palette[3 * t00 + k] + c->w * s->palette[3 * t10 + k];
        p->bottom[k] = (ONE - c->w) * s->palette[3 * t01 + k] + c->w * s->palette[3 * t11 + k];
    }
    p->top[3] = p->bottom[3] = 0;
}


/* the pair blended down by wv of ONE, dithered for screen x, y, the palette's nearest */
static uint8_t pair_blend(const HorizonSmooth *s, const Pair *p, int32_t wv, int x, int y)
{
    const int32_t d = (BAYER[y & 3][x & 3] * 2 - 15) * DITHER * ONE * ONE / 32 + ONE * ONE / 2;
    int rgb[3];
    for (int k = 0; k < 3; k++) {
        const int32_t m = ((ONE - wv) * p->top[k] + wv * p->bottom[k] + d) >> 16;   /* / (ONE * ONE) */
        rgb[k] = m < 0 ? 0 : m > 255 ? 255 : m;
    }
    return horizon_smooth_nearest(s, rgb[0], rgb[1], rgb[2]);
}

static int32_t clamp_texel(int32_t c, int32_t n)
{
    return c < 0 ? 0 : c >= n ? n - 1 : c;
}


/* inlined into the span: a call per pixel kept the blend's Pair in memory */
static inline __attribute__((always_inline)) uint8_t texel_at(const HorizonSmooth *s, const FshEntry *bitmap, int32_t u,
                                                              int32_t v, int x, int y)
{
    const int32_t w = bitmap->width, h = bitmap->height;
    const int32_t c = u >> 16, r = v >> 16;   /* floor: arithmetic shift */
    const Column col = { clamp_texel(c, w), clamp_texel(c + 1, w), (int32_t)((uint32_t)u & 0xffff) >> 8 };
    const uint8_t *row0 = bitmap->payload + (size_t)clamp_texel(r, h) * w;
    const uint8_t *row1 = bitmap->payload + (size_t)clamp_texel(r + 1, h) * w;
    Pair p;
    pair_make(s, &col, row0, row1, &p);
    return p.only >= 0 ? (uint8_t)p.only : pair_blend(s, &p, (int32_t)((uint32_t)v & 0xffff) >> 8, x, y);
}

uint8_t horizon_smooth_texel(const HorizonSmooth *s, const FshEntry *bitmap, int32_t u, int32_t v, int x, int y)
{
    return texel_at(s, bitmap, u, v, x, y);
}


void horizon_smooth_span(const HorizonSmooth *s, const FshEntry *bitmap, uint8_t *row, int32_t x0, int32_t x1, int y,
                         const double u[3], const double v[3])
{
    for (int32_t x = x0; x <= x1; x++)
        row[x] = texel_at(s, bitmap, (int32_t)floor(u[0] + u[1] * x + u[2] * y), (int32_t)floor(v[0] + v[1] * x + v[2] * y),
                          x, y);
}


/* 0043a18c's band blended: the same rows ((height - 1) x scale from y, the bitmap's from first_row), clipped */
static void blend_band(const Draw *d, const FshEntry *horz, int32_t x_offset, int32_t first_row, int32_t x, int32_t y)
{
    const HorizonTables *t = d->t;
    const int32_t w = horz->width, h = horz->height;
    if (t->scale <= 0 || w <= 0 || h <= 0) return;
    static BANDS_THREAD_LOCAL float at[HORIZON_WIDE_WIDTH + 1];   /* port (render bands): per thread */
    static BANDS_THREAD_LOCAL Column cols[HORIZON_WIDE_WIDTH + 1];
    static BANDS_THREAD_LOCAL Pair pairs[HORIZON_WIDE_WIDTH + 1];
    column_positions(t, at);
    for (int i = 0; i < t->width; i++) {   /* texel centres at k + 0.5 */
        const float fu = (float)x_offset + at[i] - 0.5f;
        const int32_t c = (int32_t)floorf(fu);
        cols[i] = (Column){ wrap(c, w), wrap(c + 1, w), (int32_t)((fu - (float)c) * ONE) };
    }
    const int32_t rows = (int32_t)(((int64_t)t->scale * (h - 1) + 0x8000) >> 16);
    const float step = 65536.0f / (float)t->scale;
    const int x0 = d->clip->x0 > x ? d->clip->x0 - x : 0;
    const int x1 = d->clip->x1 - x < t->width ? d->clip->x1 - x : t->width;
    int32_t made = INT32_MIN;   /* the bitmap row `pairs` hold */
    for (int32_t k = 0; k < rows; k++) {
        const int32_t sy = y + k;
        if (sy < d->clip->y0 || sy >= d->clip->y1) continue;
        const float fv = (float)first_row + ((float)k + 0.5f) * step - 0.5f;
        const int32_t r = (int32_t)floorf(fv), wv = (int32_t)((fv - (float)r) * ONE);
        if (r != made) {
            const uint8_t *row0 = horz->payload + (size_t)(r < 0 ? 0 : r >= h ? h - 1 : r) * w;
            const uint8_t *row1 = horz->payload + (size_t)(r + 1 < 0 ? 0 : r + 1 >= h ? h - 1 : r + 1) * w;
            for (int i = x0; i < x1; i++) pair_make(d->s, &cols[i], row0, row1, &pairs[i]);
            made = r;
        }
        uint8_t *dst = d->target->pixels + (size_t)sy * d->target->pitch + x;
        for (int i = x0; i < x1; i++)
            dst[i] = pairs[i].only >= 0 ? (uint8_t)pairs[i].only : pair_blend(d->s, &pairs[i], wv, x + i, sy);
    }
}

/* port: the band's pixels kept from the last frame. A car on a straight keeps the camera's yaw, and the band is a function of
 * the bitmap, the palette, the columns and where it falls, so the same draw again is a copy. Everything the draw reads is
 * in the key and compared whole (the bitmap, palette and columns by content, not address), so a hit is the pixels a
 * redraw would make. One slot to a render band (bands_index): a band is run by one thread at a time. */
typedef struct {
    int valid, ready;
    int32_t x_offset, first_row, x, y, scale, width, bw, bh;
    ClipRect clip;
    uint8_t palette[768];
    int32_t *columns;
    uint8_t *bitmap;
    size_t columns_cap, bitmap_cap, out_cap;
    uint8_t *out;   /* the band's rows y0 .. y1 by x0 .. x1, tight */
    int32_t out_x0, out_x1, out_y0, out_y1;
    unsigned long hits;
} BandCache;

static BandCache band_cache[BANDS_MAX];
static int cache_on = 1;

void horizon_smooth_cache_enable(int on) { cache_on = on; }

static int grow(void **p, size_t *cap, size_t need)
{
    if (need > *cap) {
        void *n = realloc(*p, need);
        if (n == NULL) return 0;
        *p = n, *cap = need;
    }
    return 1;
}

void horizon_smooth_cache_reset(void)
{
    for (int i = 0; i < BANDS_MAX; i++) {
        free(band_cache[i].columns), free(band_cache[i].bitmap), free(band_cache[i].out);
        band_cache[i] = (BandCache){ 0 };
    }
}

unsigned long horizon_smooth_cache_hits(void)
{
    unsigned long n = 0;
    for (int i = 0; i < BANDS_MAX; i++) n += band_cache[i].hits;
    return n;
}

static int same_clip(const ClipRect *a, const ClipRect *b)
{
    return a->x0 == b->x0 && a->x1 == b->x1 && a->y0 == b->y0 && a->y1 == b->y1;
}

static int cache_matches(const BandCache *c, const Draw *d, const FshEntry *horz, int32_t x_offset, int32_t first_row,
                         int32_t x, int32_t y)
{
    const HorizonTables *t = d->t;
    const size_t bitmap = (size_t)horz->width * (size_t)horz->height;
    return c->valid && c->ready == d->s->ready && c->x_offset == x_offset && c->first_row == first_row && c->x == x &&
           c->y == y && c->scale == t->scale && c->width == t->width && c->bw == horz->width && c->bh == horz->height &&
           same_clip(&c->clip, d->clip) && memcmp(c->palette, d->s->palette, sizeof c->palette) == 0 &&
           memcmp(c->columns, t->columns, (size_t)t->width * sizeof *t->columns) == 0 &&
           memcmp(c->bitmap, horz->payload, bitmap) == 0;
}

static void smooth_band(void *user, const FshEntry *horz, int32_t x_offset, int32_t first_row, int32_t x, int32_t y)
{
    const Draw *d = user;
    const HorizonTables *t = d->t;
    const int32_t w = horz->width, h = horz->height;
    if (t->scale <= 0 || w <= 0 || h <= 0 || t->width < 0 || t->width > HORIZON_WIDE_WIDTH) return;
    const int slot = bands_index();
    BandCache *c = &band_cache[slot >= 0 && slot < BANDS_MAX ? slot : 0];
    if (!cache_on) {
        blend_band(d, horz, x_offset, first_row, x, y);
        return;
    }
    if (cache_matches(c, d, horz, x_offset, first_row, x, y) && c->out != NULL) {
        const size_t span = (size_t)(c->out_x1 - c->out_x0);
        for (int32_t sy = c->out_y0; sy < c->out_y1; sy++)
            memcpy(d->target->pixels + (size_t)sy * d->target->pitch + c->out_x0, c->out + (size_t)(sy - c->out_y0) * span,
                   span);
        c->hits++;
        return;
    }
    blend_band(d, horz, x_offset, first_row, x, y);
    /* keep it: the rows and columns the draw wrote */
    const int32_t rows = (int32_t)(((int64_t)t->scale * (h - 1) + 0x8000) >> 16);
    const int32_t y0 = y > d->clip->y0 ? y : d->clip->y0, y1 = y + rows < d->clip->y1 ? y + rows : d->clip->y1;
    const int32_t x0 = x + (d->clip->x0 > x ? d->clip->x0 - x : 0);
    const int32_t x1 = x + (d->clip->x1 - x < t->width ? d->clip->x1 - x : t->width);
    c->valid = 0;
    if (y1 <= y0 || x1 <= x0) return;
    const size_t span = (size_t)(x1 - x0), bitmap = (size_t)w * (size_t)h;
    if (!grow((void **)&c->columns, &c->columns_cap, (size_t)t->width * sizeof *t->columns) ||
        !grow((void **)&c->bitmap, &c->bitmap_cap, bitmap) || !grow((void **)&c->out, &c->out_cap, span * (size_t)(y1 - y0)))
        return;
    for (int32_t sy = y0; sy < y1; sy++)
        memcpy(c->out + (size_t)(sy - y0) * span, d->target->pixels + (size_t)sy * d->target->pitch + x0, span);
    memcpy(c->columns, t->columns, (size_t)t->width * sizeof *t->columns);
    memcpy(c->bitmap, horz->payload, bitmap);
    memcpy(c->palette, d->s->palette, sizeof c->palette);
    c->ready = d->s->ready, c->x_offset = x_offset, c->first_row = first_row, c->x = x, c->y = y, c->scale = t->scale;
    c->width = t->width, c->bw = w, c->bh = h, c->clip = *d->clip;
    c->out_x0 = x0, c->out_x1 = x1, c->out_y0 = y0, c->out_y1 = y1;
    c->valid = 1;
}

void horizon_draw_flat_smooth(const HorizonSmooth *s, const HorizonTables *t, const HorizonTarget *target,
                              const ClipRect *clip, const FshEntry *horz, int32_t yaw, int mirrored, int sky_off,
                              int32_t line)
{
    if (t->width == 0 || line == INT32_MIN) return;
    const Draw d = { s, t, target, clip };
    const HorizonFlatSink sink = { (void *)&d, smooth_fill, smooth_band };
    const HorizonBox box = { clip->x0, clip->y0, clip->x1 - clip->x0, clip->y1 - clip->y0 };
    horizon_flat(horz, yaw, mirrored, sky_off, t->scale, &box, line, &sink);
}
