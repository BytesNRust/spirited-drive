/* port (spike): a near road cell drawn from its camera-space corners: clipped against the near plane rather than
 * pushed onto it, projected in floating point (no pixel snapping) and textured perspective-correct (no affine
 * warp). The exe's path is 00425718's subdivision into the affine mapper (track_draw.c, quad.c). */
#include "viewer/modern_road.h"

#include "game/gfx/quad_pixel.h"
#include "game/gfx/raster_record.h"

#include <math.h>
#include <string.h>
#if defined(__SSE2__) && !defined(__FMA__)
#include <emmintrin.h>
#endif

enum { MAX_VERTS = 8 };

/* what a polygon is painted with: a texture through a MapMode's rules, or (texture NULL) one palette colour */
typedef struct {
    const FshEntry *texture;
    Paint paint;
    uint8_t colour;
    float u_lo, u_hi, v_lo, v_hi;   /* the polygon's own uv: a pixel past its edge (EDGE_GROW) reads no texel beyond */
    const uint8_t (*lit)[256];      /* port (PS1 light): MODERN_FOG_LEVELS tables a pixel's darkness picks from; NULL none */
    float dark[4];                  /* the darkness (0 light .. MODERN_FOG_LEVELS - 1) at each corner, in cam's order */
} Fill;

static void uv_box(Fill *fill, const int32_t uv[8], int corners)
{
    fill->u_lo = fill->v_lo = 1e30f, fill->u_hi = fill->v_hi = -1e30f;
    for (int i = 0; i < corners; i++) {
        const float u = uv[2 * i] / 65536.0f, v = uv[2 * i + 1] / 65536.0f;
        fill->u_lo = fminf(fill->u_lo, u), fill->u_hi = fmaxf(fill->u_hi, u);
        fill->v_lo = fminf(fill->v_lo, v), fill->v_hi = fmaxf(fill->v_hi, v);
    }
}

typedef struct {
    float x, y, z, u, v, l;
} Vert;

typedef struct {
    float sx, sy, iz, uz, vz, lz;
} Screen;

static Vert lerp(Vert a, Vert b, float t)
{
    return (Vert){ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.u + (b.u - a.u) * t,
                   a.v + (b.v - a.v) * t, a.l + (b.l - a.l) * t };
}

static int clip_near(const Vert *in, int count, float near_z, Vert *out)
{
    int n = 0;
    for (int i = 0; i < count; i++) {
        const Vert a = in[i], b = in[(i + 1) % count];
        const int a_in = a.z >= near_z, b_in = b.z >= near_z;
        if (a_in) out[n++] = a;
        if (a_in != b_in) out[n++] = lerp(a, b, (near_z - a.z) / (b.z - a.z));
    }
    return n;
}

static float edge(const Screen *a, const Screen *b, float x, float y)
{
    return (b->sx - a->sx) * (y - a->sy) - (b->sy - a->sy) * (x - a->sx);
}

static ModernStyle style;   /* none: filter 0, fog NULL */

void modern_road_set_style(const ModernStyle *s)
{
    style = s != NULL ? *s : (ModernStyle){ 0, NULL, 0, 0 };
}

void modern_fog_tables(const uint8_t palette[768], const uint8_t rgb[3], uint8_t out[MODERN_FOG_LEVELS][256])
{
    enum { CHOICES = 0xfe };   /* 0xfe and 0xff mark shadow and see-through texels */
    for (int c = 0; c < 256; c++) out[0][c] = (uint8_t)c;
    for (int l = 1; l < MODERN_FOG_LEVELS; l++) {
        const float t = (float)l / (MODERN_FOG_LEVELS - 1);
        for (int c = 0; c < 256; c++) {
            float want[3];
            for (int k = 0; k < 3; k++) want[k] = palette[3 * c + k] + (rgb[k] - palette[3 * c + k]) * t;
            int best = 0;
            float best_d = 1e30f;
            for (int i = 0; i < CHOICES; i++) {
                const float dr = palette[3 * i] - want[0], dg = palette[3 * i + 1] - want[1], db = palette[3 * i + 2] - want[2];
                const float d = dr * dr + dg * dg + db * db;
                if (d < best_d) best = i, best_d = d;
            }
            out[l][c] = (uint8_t)best;
        }
    }
}

void modern_light_tables(const uint8_t palette[768], const float tint[3], uint8_t out[MODERN_FOG_LEVELS][256])
{
    enum { CHOICES = 0xfe };   /* 0xfe and 0xff mark shadow and see-through texels: they stay */
    for (int l = 0; l < MODERN_FOG_LEVELS; l++) {
        const float f = 1.0f - (float)l / (MODERN_FOG_LEVELS - 1);
        for (int c = 0; c < 256; c++) {
            if (c >= CHOICES || (l == 0 && tint[0] == 1.0f && tint[1] == 1.0f && tint[2] == 1.0f)) {
                out[l][c] = (uint8_t)c;
                continue;
            }
            float want[3];
            for (int k = 0; k < 3; k++) want[k] = fminf(palette[3 * c + k] * tint[k] * f, 255.0f);
            int best = 0;
            float best_d = 1e30f;
            for (int i = 0; i < CHOICES; i++) {
                const float dr = palette[3 * i] - want[0], dg = palette[3 * i + 1] - want[1], db = palette[3 * i + 2] - want[2];
                const float d = dr * dr + dg * dg + db * db;
                if (d < best_d) best = i, best_d = d;
            }
            out[l][c] = (uint8_t)best;
        }
    }
}

/* the 4x4 ordered dither's thresholds, (BAYER + 0.5) / 16 by (y & 3, x & 3) */
static const float BAYER[4][4] = {
    { 0.5f / 16, 8.5f / 16, 2.5f / 16, 10.5f / 16 },
    { 12.5f / 16, 4.5f / 16, 14.5f / 16, 6.5f / 16 },
    { 3.5f / 16, 11.5f / 16, 1.5f / 16, 9.5f / 16 },
    { 15.5f / 16, 7.5f / 16, 13.5f / 16, 5.5f / 16 },
};

/* the fog's level at camera depth z for pixel x, y: between two levels a dither of both, so no bands */
static int fog_level(float z, int x, int y)
{
    const float t = fminf(fmaxf((z - style.fog_start) / (style.fog_end - style.fog_start), -1), 2);
    const int l = (int)floorf(t * (MODERN_FOG_LEVELS - 1) + BAYER[y & 3][x & 3]);
    return l < 0 ? 0 : l >= MODERN_FOG_LEVELS ? MODERN_FOG_LEVELS - 1 : l;
}

/* port (PS1 light): the level of a pixel whose interpolated darkness is d, a dither between two levels as the fog's */
static int dark_level(float d, int x, int y)
{
    const int l = (int)floorf(d + BAYER[y & 3][x & 3]);
    return l < 0 ? 0 : l >= MODERN_FOG_LEVELS ? MODERN_FOG_LEVELS - 1 : l;
}

#if defined(__SSE2__) && !defined(__FMA__)
/* fog_level for the depths z of four pixels x .. x + 3 of row y: the same operations, a lane each */
static inline __m128i fog_levels(__m128 z, int x, int y, const float fog_start, const float fog_span)
{
    const __m128 t = _mm_min_ps(_mm_max_ps(_mm_div_ps(_mm_sub_ps(z, _mm_set1_ps(fog_start)), _mm_set1_ps(fog_span)),
                                           _mm_set1_ps(-1)),
                                _mm_set1_ps(2));
    const float *b = BAYER[y & 3];
    const __m128 f = _mm_add_ps(_mm_mul_ps(t, _mm_set1_ps(MODERN_FOG_LEVELS - 1)),
                                _mm_setr_ps(b[x & 3], b[(x + 1) & 3], b[(x + 2) & 3], b[(x + 3) & 3]));
    __m128i l = _mm_cvttps_epi32(f);   /* floor: the truncation, one less where f was below it */
    l = _mm_add_epi32(l, _mm_castps_si128(_mm_cmplt_ps(f, _mm_cvtepi32_ps(l))));
    const __m128i top = _mm_set1_epi32(MODERN_FOG_LEVELS - 1);
    l = _mm_andnot_si128(_mm_cmplt_epi32(l, _mm_setzero_si128()), l);
    const __m128i over = _mm_cmpgt_epi32(l, top);
    return _mm_or_si128(_mm_and_si128(over, top), _mm_andnot_si128(over, l));
}
#endif

/* the filter's offsets in texels, by (x & 1) | (y & 1) << 1: the 2x2 ordered pattern, centred on 0 */
static const float FILTER[4][2] = { { -0.125f, -0.375f }, { 0.125f, 0.375f }, { 0.375f, 0.125f }, { -0.375f, -0.125f } };

static uint8_t texel(const Fill *fill, float u, float v)
{
    const FshEntry *t = fill->texture;
    u = fminf(fmaxf(u, fill->u_lo), fill->u_hi), v = fminf(fmaxf(v, fill->v_lo), fill->v_hi);
    int x = (int)floorf(u), y = (int)floorf(v);
    x = x < 0 ? 0 : x >= t->width ? t->width - 1 : x;
    y = y < 0 ? 0 : y >= t->height ? t->height - 1 : y;
    return t->payload[(size_t)y * t->width + x];
}

/* a barycentric weight along a row: w(px) = step * px + at; `lo`..`hi` narrowed to the pixel centres where it is
 * not below -SPAN_SLACK (the slack keeps shared edges covered by both triangles: no cracks); 0 when none are */
static const float SPAN_SLACK = 1e-5f;
/* a polygon's outer edges reach this much of a pixel further out: a T junction (a strip's edge straight beside its
 * neighbours' bent through a point of a finer level) leaves a sliver under a pixel wide, which the exe's mapper rounds
 * over and the exact edges here would show as a broken line; under half a pixel, a polygon on pixel edges grows by none */
static const float EDGE_GROW = 0.45f;
/* 1 / z at the least: a pixel past an edge (EDGE_GROW) of a polygon seen nearly edge on extrapolates past its plane's
 * vanishing line, where 1 / z reaches 0 */
static const float IZ_MIN = 1e-9f;

static int narrow(float step, float at, float slack, float *lo, float *hi)
{
    const float bound = (-slack - at);
    if (step > 0) *lo = fmaxf(*lo, bound / step);
    else if (step < 0) *hi = fminf(*hi, bound / step);
    else if (at < -SPAN_SLACK) return 0;
    return *lo <= *hi;
}

/* the attribute planes: a value at a pixel centre is base + px * dx + py * dy */
typedef struct {
    float base, dx, dy;
} Plane;

static Plane plane(const float c[3][2], float a0, float a1, float a2)
{
    /* attribute = a2 + w0 (a0 - a2) + w1 (a1 - a2), with w = c[i][0] px + c[i][1] py + c[i][2] */
    const float d0 = a0 - a2, d1 = a1 - a2;
    return (Plane){ 0, c[0][0] * d0 + c[1][0] * d1, c[0][1] * d0 + c[1][1] * d1 };
}

/* one row's span: the pixels whose centres all three weights cover, its attributes stepped along it with one divide
 * per pixel for the perspective */
static void span(const QuadTarget *q, int y, int x0, int x1, const float w[3][3], const float slack[3], const Plane at[4],
                 const Fill *fill)
{
    const float py = y + 0.5f;
    float lo = x0 + 0.5f, hi = x1 + 0.5f;
    for (int i = 0; i < 3; i++)
        if (!narrow(w[i][0], w[i][1] * py + w[i][2], slack[i], &lo, &hi)) return;
    const int xs = (int)ceilf(lo - 0.5f), xe = (int)floorf(hi - 0.5f);
    if (xs > xe) return;
    uint8_t *row = q->pixels + (size_t)y * q->pitch;
    const float px = xs + 0.5f;
    const float iz = at[0].base + at[0].dx * px + at[0].dy * py, uz = at[1].base + at[1].dx * px + at[1].dy * py,
                vz = at[2].base + at[2].dx * px + at[2].dy * py;
    if (fill->texture == NULL) {
        if (style.fog == NULL) {
            memset(row + xs, fill->colour, (size_t)(xe - xs + 1));
            return;
        }
        int x = xs;
#if defined(__SSE2__) && !defined(__FMA__)
        for (; x + 3 <= xe; x += 4) {   /* four pixels at a time, as below */
            const __m128 k = _mm_add_ps(_mm_set1_ps((float)(x - xs)), _mm_setr_ps(0, 1, 2, 3));
            const __m128 z = _mm_div_ps(_mm_set1_ps(1.0f),
                                        _mm_max_ps(_mm_add_ps(_mm_set1_ps(iz), _mm_mul_ps(_mm_set1_ps(at[0].dx), k)),
                                                   _mm_set1_ps(IZ_MIN)));
            int32_t l[4];
            _mm_storeu_si128((__m128i *)l, fog_levels(z, x, y, style.fog_start, style.fog_end - style.fog_start));
            for (int j = 0; j < 4; j++) row[x + j] = style.fog[l[j]][fill->colour];
        }
#endif
        for (; x <= xe; x++)
            row[x] = style.fog[fog_level(1.0f / fmaxf(iz + at[0].dx * (float)(x - xs), IZ_MIN), x, y)][fill->colour];
        return;
    }
    /* the loop's inputs in locals: the stores to the row may alias the globals and the planes, which would reload them
     * for every pixel */
    const float izdx = at[0].dx, izdy = at[0].dy, udx = at[1].dx, udy = at[1].dy, vdx = at[2].dx, vdy = at[2].dy;
    const int filter = style.filter;
    const uint8_t (*fog)[256] = style.fog;
    const uint8_t (*lit)[256] = fill->lit;
    const float lz = at[3].base + at[3].dx * px + at[3].dy * py, lzdx = at[3].dx;
    int x = xs;
#if defined(__SSE2__) && !defined(__FMA__)
    /* four pixels at a time through the perspective: the same IEEE operations in the same order, so the same floats
     * (no fused multiply-add on this target), then each lane's texel as below */
    const __m128 abs_mask = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff)), one = _mm_set1_ps(1.0f);
    for (; x + 3 <= xe; x += 4) {
        const __m128 k = _mm_add_ps(_mm_set1_ps((float)(x - xs)), _mm_setr_ps(0, 1, 2, 3));
        const __m128 rv = _mm_div_ps(one, _mm_max_ps(_mm_add_ps(_mm_set1_ps(iz), _mm_mul_ps(_mm_set1_ps(izdx), k)),
                                                     _mm_set1_ps(IZ_MIN)));
        __m128 uv = _mm_mul_ps(_mm_add_ps(_mm_set1_ps(uz), _mm_mul_ps(_mm_set1_ps(udx), k)), rv);
        __m128 vv = _mm_mul_ps(_mm_add_ps(_mm_set1_ps(vz), _mm_mul_ps(_mm_set1_ps(vdx), k)), rv);
        if (filter) {
            const __m128 izx = _mm_set1_ps(izdx), izy = _mm_set1_ps(izdy);
            const __m128 du_x = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(udx), _mm_mul_ps(uv, izx)), rv);
            const __m128 dv_x = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(vdx), _mm_mul_ps(vv, izx)), rv);
            const __m128 du_y = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(udy), _mm_mul_ps(uv, izy)), rv);
            const __m128 dv_y = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(vdy), _mm_mul_ps(vv, izy)), rv);
            const __m128 over = _mm_and_ps(
                _mm_and_ps(_mm_cmplt_ps(_mm_and_ps(du_x, abs_mask), one), _mm_cmplt_ps(_mm_and_ps(dv_x, abs_mask), one)),
                _mm_and_ps(_mm_cmplt_ps(_mm_and_ps(du_y, abs_mask), one), _mm_cmplt_ps(_mm_and_ps(dv_y, abs_mask), one)));
            const float *e = FILTER[(x & 1) | (y & 1) << 1], *o = FILTER[(~x & 1) | (y & 1) << 1];
            const __m128 ou = _mm_setr_ps(e[0], o[0], e[0], o[0]), ov = _mm_setr_ps(e[1], o[1], e[1], o[1]);
            uv = _mm_or_ps(_mm_and_ps(over, _mm_add_ps(uv, ou)), _mm_andnot_ps(over, uv));
            vv = _mm_or_ps(_mm_and_ps(over, _mm_add_ps(vv, ov)), _mm_andnot_ps(over, vv));
        }
        float us[4], vs[4];
        int32_t lv[4] = { 0 };
        float rs[4];
        _mm_storeu_ps(us, uv), _mm_storeu_ps(vs, vv), _mm_storeu_ps(rs, rv);
        if (fog != NULL)
            _mm_storeu_si128((__m128i *)lv, fog_levels(rv, x, y, style.fog_start, style.fog_end - style.fog_start));
        for (int j = 0; j < 4; j++) {
            uint8_t c = texel(fill, us[j], vs[j]);
            if (fog != NULL && c < MAP_SHADE_TEXEL) c = fog[lv[j]][c];
            if (lit != NULL && c < MAP_SHADE_TEXEL) c = lit[dark_level((lz + lzdx * (float)(x + j - xs)) * rs[j], x + j, y)][c];
            paint_texel(&fill->paint, &row[x + j], c);
        }
    }
#endif
    for (; x <= xe; x++) {
        const float k = (float)(x - xs), r = 1.0f / fmaxf(iz + izdx * k, IZ_MIN);
        float u = (uz + udx * k) * r, v = (vz + vdx * k) * r;
        if (filter) {
            /* a texel's span across and down the screen at this pixel: the filter only where it is over a pixel */
            const float du_x = (udx - u * izdx) * r, dv_x = (vdx - v * izdx) * r;
            const float du_y = (udy - u * izdy) * r, dv_y = (vdy - v * izdy) * r;
            if (fabsf(du_x) < 1 && fabsf(dv_x) < 1 && fabsf(du_y) < 1 && fabsf(dv_y) < 1) {
                const float *o = FILTER[(x & 1) | (y & 1) << 1];
                u += o[0], v += o[1];
            }
        }
        uint8_t c = texel(fill, u, v);
        if (fog != NULL && c < MAP_SHADE_TEXEL) c = fog[fog_level(r, x, y)][c];
        if (lit != NULL && c < MAP_SHADE_TEXEL) c = lit[dark_level((lz + lzdx * k) * r, x, y)][c];
        paint_texel(&fill->paint, &row[x], c);
    }
}

/* outer: bit i set when the edge opposite corner i is the polygon's own (not a diagonal of its fan) */
static void triangle(const QuadTarget *q, const Screen *p0, const Screen *p1, const Screen *p2, const Fill *fill,
                     unsigned outer)
{
    const float area = edge(p0, p1, p2->sx, p2->sy);
    if (fabsf(area) < 1e-6f) return;
    const float lo_x = fminf(p0->sx, fminf(p1->sx, p2->sx)) - 1, hi_x = fmaxf(p0->sx, fmaxf(p1->sx, p2->sx)) + 1;
    const float lo_y = fminf(p0->sy, fminf(p1->sy, p2->sy)) - 1, hi_y = fmaxf(p0->sy, fmaxf(p1->sy, p2->sy)) + 1;
    const int x0 = (int)fmaxf((float)q->clip_left, floorf(lo_x)), x1 = (int)fminf((float)q->clip_right - 1, ceilf(hi_x));
    const int y0 = (int)fmaxf((float)q->clip_top, floorf(lo_y)), y1 = (int)fminf((float)q->clip_bottom - 1, ceilf(hi_y));
    /* w0 = edge(p1, p2) / area, w1 = edge(p2, p0) / area as planes in px, py; w2 = 1 - w0 - w1 */
    const float inv = 1.0f / area;
    float w[3][3] = {
        { -(p2->sy - p1->sy) * inv, (p2->sx - p1->sx) * inv, 0 },
        { -(p0->sy - p2->sy) * inv, (p0->sx - p2->sx) * inv, 0 },
        { 0, 0, 0 }
    };
    w[0][2] = -(w[0][0] * p1->sx + w[0][1] * p1->sy);
    w[1][2] = -(w[1][0] * p2->sx + w[1][1] * p2->sy);
    for (int k = 0; k < 3; k++) w[2][k] = (k == 2 ? 1.0f : 0.0f) - w[0][k] - w[1][k];
    const float c[3][2] = { { w[0][0], w[0][1] }, { w[1][0], w[1][1] }, { 0, 0 } };
    Plane at[4] = { plane(c, p0->iz, p1->iz, p2->iz), plane(c, p0->uz, p1->uz, p2->uz),
                    plane(c, p0->vz, p1->vz, p2->vz), plane(c, p0->lz, p1->lz, p2->lz) };
    const float a2[4] = { p2->iz, p2->uz, p2->vz, p2->lz }, a0[4] = { p0->iz, p0->uz, p0->vz, p0->lz },
                a1[4] = { p1->iz, p1->uz, p1->vz, p1->lz };
    for (int k = 0; k < 4; k++) at[k].base = a2[k] + w[0][2] * (a0[k] - a2[k]) + w[1][2] * (a1[k] - a2[k]);
    /* a shade texel darkens what is under it: drawn twice where two polygons meet, a dark seam; those keep their edges */
    const int grow = fill->texture == NULL || fill->paint.mode != MAP_SHADE;
    float slack[3];
    for (int i = 0; i < 3; i++)   /* a weight's distance from its edge is the weight over its gradient's length */
        slack[i] = SPAN_SLACK + (grow && (outer >> i & 1) ? EDGE_GROW * hypotf(w[i][0], w[i][1]) : 0);
    for (int y = y0; y <= y1; y++) span(q, y, x0, x1, w, slack, at, fill);
}

static void draw_clipped(const QuadTarget *q, const View *view, const int32_t uv[8], const int32_t *const cam[4],
                         int corners, const Fill *fill)
{
    Vert in[4], out[MAX_VERTS];
    for (int i = 0; i < corners; i++)
        in[i] = (Vert){ (float)cam[i][0], (float)cam[i][1], (float)cam[i][2], uv[2 * i] / 65536.0f,
                        uv[2 * i + 1] / 65536.0f, fill->lit != NULL ? fill->dark[i] : 0 };
    const int n = clip_near(in, corners, (float)view->near_z, out);
    if (n < 3) return;
    Screen s[MAX_VERTS];
    const float focal = (float)view->focal_x;  /* 00475eac scales both axes by focal_x's reciprocals */
    for (int i = 0; i < n; i++) {
        const float iz = 1.0f / out[i].z;
        s[i] = (Screen){ view->centre_x + out[i].x * focal * iz, view->centre_y - out[i].y * focal * iz, iz,
                         out[i].u * iz, out[i].v * iz, out[i].l * iz };
    }
    /* the fan's triangle i: its edge i..i+1 the polygon's, 0..i only for the first, i+1..0 only for the last */
    for (int i = 1; i + 1 < n; i++)
        triangle(q, &s[0], &s[i], &s[i + 1], fill, 1u | (i + 2 == n ? 2u : 0) | (i == 1 ? 4u : 0));
}

/* port (render bands): the three polygon draws recorded as their arguments and replayed over a band's rows (each row
 * is computed from absolute coordinates, so a narrowed clip is exact); the view is kept by pointer, the style is the
 * frame's */
static void keep(RasterCmd *c, const View *view, const int32_t uv[8], const int32_t *const cam[4], int corners)
{
    c->table = view, c->count = corners;
    memcpy(c->v, uv, 8 * sizeof *uv);
    for (int i = 0; i < corners; i++) memcpy(c->v + 8 + 3 * i, cam[i], 3 * sizeof *cam[i]);
}

static void cam_of(const RasterCmd *c, const int32_t *cam[4])
{
    for (int i = 0; i < 4; i++) cam[i] = c->v + 8 + 3 * i;
}

static void replay_cell(const RasterCmd *c, const QuadTarget *whole, int32_t top, int32_t bottom)
{
    QuadTarget band;
    const int32_t *cam[4];
    cam_of(c, cam);
    if (raster_narrow(whole, top, bottom, &band)) modern_road_cell(&band, c->table, c->texture, c->v, cam, (int)c->mode);
}

static void replay_polygon(const RasterCmd *c, const QuadTarget *whole, int32_t top, int32_t bottom)
{
    QuadTarget band;
    const int32_t *cam[4];
    cam_of(c, cam);
    if (raster_narrow(whole, top, bottom, &band))
        modern_polygon(&band, c->table, c->texture, c->v, cam, c->count, (MapMode)c->mode, c->shape);
}

static void replay_flat(const RasterCmd *c, const QuadTarget *whole, int32_t top, int32_t bottom)
{
    QuadTarget band;
    const int32_t *cam[4];
    cam_of(c, cam);
    if (raster_narrow(whole, top, bottom, &band)) modern_flat(&band, c->table, (uint8_t)c->colour, cam, c->count);
}

void modern_road_cell(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8],
                      const int32_t *const cam[4], int transparent)
{
    struct RasterRecorder *rec = raster_recorder_for(q);
    if (rec != NULL) {
        RasterCmd *c = raster_record_add(rec, q, replay_cell);
        c->texture = texture, c->mode = transparent;
        keep(c, view, uv, cam, 4);
        return;
    }
    Fill fill = { texture, { transparent ? MAP_TRANSPARENT : MAP_OPAQUE, NULL }, 0, 0, 0, 0, 0, NULL, { 0 } };
    uv_box(&fill, uv, 4);
    draw_clipped(q, view, uv, cam, 4, &fill);
}

/* port (PS1 light): a road cell whose corners carry a darkness, drawn through `tables` a pixel at a time (replay_lit keeps
 * the four floats in the last four words of the command) */
static void replay_lit(const RasterCmd *c, const QuadTarget *whole, int32_t top, int32_t bottom)
{
    QuadTarget band;
    const int32_t *cam[4];
    float dark[4];
    cam_of(c, cam);
    memcpy(dark, c->v + 20, sizeof dark);
    if (raster_narrow(whole, top, bottom, &band))
        modern_road_cell_lit(&band, c->table, c->texture, c->v, cam, (int)c->mode, (const uint8_t (*)[256])c->shape, dark);
}

void modern_road_cell_lit(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8],
                          const int32_t *const cam[4], int transparent, const uint8_t (*tables)[256], const float dark[4])
{
    struct RasterRecorder *rec = raster_recorder_for(q);
    if (rec != NULL) {
        RasterCmd *c = raster_record_add(rec, q, replay_lit);
        c->texture = texture, c->mode = transparent, c->shape = (const uint8_t *)tables;
        keep(c, view, uv, cam, 4);
        memcpy(c->v + 20, dark, 4 * sizeof *dark);
        return;
    }
    Fill fill = { texture, { transparent ? MAP_TRANSPARENT : MAP_OPAQUE, NULL }, 0, 0, 0, 0, 0, tables,
                  { dark[0], dark[1], dark[2], dark[3] } };
    uv_box(&fill, uv, 4);
    draw_clipped(q, view, uv, cam, 4, &fill);
}

void modern_polygon(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8],
                    const int32_t *const cam[4], int corners, MapMode mode, const uint8_t *shade)
{
    if (texture == NULL || (corners != 3 && corners != 4)) return;
    Fill fill = { texture, { mode, shade }, 0, 0, 0, 0, 0, NULL, { 0 } };
    if (!paint_valid(&fill.paint)) return;
    struct RasterRecorder *rec = raster_recorder_for(q);
    if (rec != NULL) {
        RasterCmd *c = raster_record_add(rec, q, replay_polygon);
        c->texture = texture, c->mode = mode, c->shape = shade;
        keep(c, view, uv, cam, corners);
        return;
    }
    uv_box(&fill, uv, corners);
    draw_clipped(q, view, uv, cam, corners, &fill);
}

void modern_flat(const QuadTarget *q, const View *view, uint8_t colour, const int32_t *const cam[4], int corners)
{
    if (corners != 3 && corners != 4) return;
    const int32_t uv[8] = { 0 };
    struct RasterRecorder *rec = raster_recorder_for(q);
    if (rec != NULL) {
        RasterCmd *c = raster_record_add(rec, q, replay_flat);
        c->colour = colour;
        keep(c, view, uv, cam, corners);
        return;
    }
    const Fill fill = { NULL, { MAP_OPAQUE, NULL }, colour, 0, 0, 0, 0, NULL, { 0 } };
    draw_clipped(q, view, uv, cam, corners, &fill);
}
