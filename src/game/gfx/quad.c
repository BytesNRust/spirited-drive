/* Affine textured-quad mapper. Ported from NFS_WIN.EXE 00487180 (opaque) and
 * 00486950 (colour 0xFF transparent); the two are the same function but for the
 * scanline routine they call (004a070c and 004a02d8).
 *
 * The mapper walks the polygon outline once or twice into per-scanline tables
 * (left and right x, u and v in 16.16), then for each row steps u and v by a
 * constant per-pixel increment. Rows are visited top to bottom; the original
 * keeps its tables on the stack uninitialised, here they start at zero, which
 * only matters for self-intersecting quads that leave rows unwritten. */
#include "game/gfx/quad.h"

#include "game/gfx/x86_int.h"

#include <stddef.h>
#include <string.h>

enum {
    ROWS = 768,            /* entries in each per-scanline table (0x300) */
    TEXEL_BITS = 8,        /* bits per texel and per surface pixel */
    TRANSPARENT_TEXEL = 0xFF,
    FSH_TYPE_BITMAP_TRANSPARENT = 0x45
};

typedef struct {
    int32_t x_left[ROWS];   /* local_4874 */
    int32_t u_left[ROWS];   /* local_3c74 */
    int32_t x_right[ROWS];  /* local_3074 */
    int32_t v_right[ROWS];  /* local_2474 */
    int32_t u_right[ROWS];  /* local_1874 */
    int32_t v_left[ROWS];   /* local_c74 */
} Edges;

typedef struct {
    const FshEntry *texture;
    int32_t row_bytes;  /* 0049f530: bytes per texture row */
} Texture;

typedef struct {
    int32_t u, du, v, dv;  /* 16.16 texel position and per-pixel step */
    int32_t step_plain;    /* texel offset change per pixel without a v carry */
    int32_t step_carry;    /* ... with one */
    int32_t count;
} Span;

static int target_valid(const QuadTarget *t)
{
    return t != NULL && t->pixels != NULL && t->pitch > 0 && t->clip_left >= 0 &&
           t->clip_left < t->clip_right && t->clip_top >= 0 && t->clip_top < t->clip_bottom &&
           t->clip_bottom <= ROWS;
}

static int texture_valid(const FshEntry *e)
{
    return e != NULL && e->payload != NULL &&
           (e->type == FSH_TYPE_BITMAP || e->type == FSH_TYPE_BITMAP_TRANSPARENT);
}

static uint8_t texel_at(const FshEntry *e, int64_t offset)
{
    return offset >= 0 && (uint64_t)offset < e->payload_size ? e->payload[offset] : 0;
}

/* 0049f530: (bits * width + 7) >> 3, the width read as a signed 16-bit field. */
static int32_t texture_row_bytes(const FshEntry *e)
{
    return x_sar(x_add(x_mul(TEXEL_BITS, (int16_t)e->width), 7), 3);
}

/* The shared prologue of the four edge routines: clips the run of n rows
 * starting at *y to the clip rectangle's top and bottom, advancing *acc by
 * step per row skipped. Returns the rows left. */
static int32_t clip_run(const QuadTarget *t, int32_t *y, int32_t n, int32_t *acc, int32_t step)
{
    if (*y < t->clip_top) {
        const int32_t skipped = t->clip_top - *y;
        n -= skipped;
        *acc = x_add(*acc, x_mul(step, skipped));
        *y = t->clip_top;
    }
    if (x_add(*y, n) >= t->clip_bottom) n = t->clip_bottom - *y;
    return n;
}

/* 004a0450: x of the left edge per row, rounded down. Returns the 16.16 slope,
 * 0 when the edge is entirely above or below the clip rectangle. */
static int32_t left_x_edge(const QuadTarget *t, int32_t *table, int32_t y, int32_t n, int32_t x0,
                           int32_t x1)
{
    if (y >= t->clip_bottom || x_add(y, n) < t->clip_top) return 0;
    const int32_t slope = x_div(x_shl(x_sub(x1, x0), 16), n);
    int32_t acc = x_shl(x0, 16);
    if (slope < 0) acc = x_add(acc, slope);
    n = clip_run(t, &y, n, &acc, slope);
    for (; n > 0; n--, y++) {
        table[y] = x_sar(acc, 16);
        acc = x_add(acc, slope);
    }
    return slope;
}

/* 004a0504: x of the right edge per row. */
static int32_t right_x_edge(const QuadTarget *t, int32_t *table, int32_t y, int32_t n, int32_t x0,
                            int32_t x1)
{
    if (y >= t->clip_bottom || x_add(y, n) < t->clip_top) return 0;
    const int32_t slope = x_div(x_shl(x_sub(x1, x0), 16), n);
    int32_t acc = x_shl(x0, 16);
    if (slope > 0) acc = x_add(acc, slope);
    n = clip_run(t, &y, n, &acc, slope);
    for (; n > 0; n--, y++) {
        table[y] = x_sar(acc, 16);
        acc = x_add(acc, slope);
    }
    return slope;
}

/* 004a05a4 (left edge, bias when the x slope is negative) and 004a0664 (right
 * edge, bias when it is positive): u or v per row along an edge, 16.16. */
static void uv_edge(const QuadTarget *t, int32_t *table, int32_t y, int32_t n, int32_t a0,
                    int32_t a1, int32_t x_slope, int left)
{
    if (y >= t->clip_bottom || x_add(y, n) < t->clip_top) return;
    const int32_t step = x_div(x_sub(a1, a0), n);
    if (left ? x_slope < 0 : x_slope > 0) a0 = x_add(a0, step);
    n = clip_run(t, &y, n, &a0, step);
    for (; n > 0; n--, y++) {
        table[y] = a0;
        a0 = x_add(a0, step);
    }
}

typedef struct {
    const QuadTarget *target;
    const int32_t *screen;  /* 4 x (x, y) */
    const int32_t *uv;      /* 4 x (u, v) */
    Edges *edges;
    int top_corner, bottom_corner;
} Outline;

/* Walks corners from the top one towards the bottom one in direction step
 * (+1 or -1 around the quad), filling the left-edge tables. First loop of
 * 00487180 and the first of the two repeated after a flip. */
static void trace_left(const Outline *o, int step)
{
    unsigned c = (unsigned)o->top_corner;
    if (o->top_corner == o->bottom_corner) return;
    do {
        const unsigned from = c;
        c = (c - (unsigned)step) & 3;
        const int32_t *p = o->screen + 2 * from, *q = o->screen + 2 * c;
        const int32_t n = x_add(x_sub(q[1], p[1]), 1);
        if (n > 0) {
            const int32_t slope = left_x_edge(o->target, o->edges->x_left, p[1], n, p[0], q[0]);
            const int32_t *a = o->uv + 2 * from, *b = o->uv + 2 * c;
            uv_edge(o->target, o->edges->u_left, p[1], n, a[0], b[0], slope, 1);
            uv_edge(o->target, o->edges->v_left, p[1], n, a[1], b[1], slope, 1);
        }
    } while (c != (unsigned)o->bottom_corner);
}

/* The right-edge counterpart, walking the other way round. */
static void trace_right(const Outline *o, int step)
{
    unsigned c = (unsigned)o->top_corner;
    if (o->top_corner == o->bottom_corner) return;
    do {
        const unsigned from = c;
        c = ((unsigned)step + c) & 3;
        const int32_t *p = o->screen + 2 * from, *q = o->screen + 2 * c;
        const int32_t n = x_add(x_sub(q[1], p[1]), 1);
        if (n > 0) {
            const int32_t slope = right_x_edge(o->target, o->edges->x_right, p[1], n, p[0], q[0]);
            const int32_t *a = o->uv + 2 * from, *b = o->uv + 2 * c;
            uv_edge(o->target, o->edges->u_right, p[1], n, a[0], b[0], slope, 0);
            uv_edge(o->target, o->edges->v_right, p[1], n, a[1], b[1], slope, 0);
        }
    } while (c != (unsigned)o->bottom_corner);
}

static void trace_outline(const Outline *o, int step)
{
    trace_left(o, step);
    trace_right(o, step);
}

/* 004a070c and 004a02d8: count pixels along a row. The texel offset advances by
 * the carry of u (one texel) plus step_carry or step_plain depending on the
 * carry of v; the original accumulates those 16-bit fractions in registers and
 * reads one texel ahead through them, which comes to the same thing. The
 * original stores whole dwords in the opaque case, the bytes are the same. */
static void draw_span(uint8_t *dst, const FshEntry *tex, int64_t offset, const Span *s,
                      int transparent)
{
    uint32_t u_frac = (uint16_t)s->u, v_frac = (uint16_t)s->v;
    const uint32_t du = (uint16_t)s->du, dv = (uint16_t)s->dv;
    for (int32_t i = 0; i < s->count; i++) {
        const uint8_t texel = texel_at(tex, offset);
        if (!transparent || texel != TRANSPARENT_TEXEL) dst[i] = texel;
        u_frac += du;
        v_frac += dv;
        offset += (int64_t)(u_frac >> 16) + ((v_frac >> 16) ? s->step_carry : s->step_plain);
        u_frac &= 0xFFFF;
        v_frac &= 0xFFFF;
    }
}

/* Offset in texels-as-bytes of the texel at 16.16 position (u, v). */
static int64_t texel_offset(const Texture *tex, int32_t u, int32_t v)
{
    const int32_t col = x_div8(x_mul(x_sar(u, 16), TEXEL_BITS));
    return (int64_t)col + x_mul(x_sar(v, 16), tex->row_bytes);
}

/* Per-row setup in 00487180: clips the row's span horizontally, derives the
 * u and v steps and the two texel offset steps, and draws it. */
static void draw_row(const QuadTarget *t, const Texture *tex, Edges *e, int32_t y, int32_t width,
                     int transparent)
{
    int32_t left = e->x_left[y];
    int32_t u = e->u_left[y], v = e->v_left[y];
    const int32_t du = x_div(x_sub(e->u_right[y], u), width);
    const int32_t dv = x_div(x_sub(e->v_right[y], v), width);
    if (left < t->clip_left) {
        const int32_t skipped = t->clip_left - left;
        u = x_add(u, x_mul(du, skipped));
        v = x_add(v, x_mul(dv, skipped));
        width -= skipped;
        e->x_left[y] = left = t->clip_left;
    }
    if (e->x_right[y] >= t->clip_right) width -= (e->x_right[y] - t->clip_right) + 1;

    const int32_t step = x_add(x_mul(x_sar(dv, 16), tex->row_bytes),
                               x_div8(x_mul(x_sar(du, 16), TEXEL_BITS)));
    const Span span = {
        .u = u, .du = du, .v = v, .dv = dv,
        .step_plain = x_div(x_mul(step, 8), TEXEL_BITS),
        .step_carry = x_div(x_mul(x_add(step, tex->row_bytes), 8), TEXEL_BITS),
        .count = width,
    };
    uint8_t *dst = t->pixels + (size_t)y * (size_t)t->pitch + left;
    draw_span(dst, tex->texture, texel_offset(tex, u, v), &span, transparent);
}

static int span_visible(const QuadTarget *t, const Edges *e, int32_t y)
{
    return e->x_left[y] < t->clip_right && t->clip_left <= e->x_right[y];
}

/* Picks the topmost and bottommost corner (first loop of 00487180, including
 * its quirk of starting from corner 0 as top and corner 2 as bottom). */
static void find_extremes(const int32_t screen[8], int *top, int *bottom, int32_t *y_top,
                          int32_t *y_bottom)
{
    *top = 0;
    *bottom = 2;
    *y_top = screen[1];
    *y_bottom = screen[5];
    for (int i = 0; i < 4; i++) {
        const int32_t y = screen[2 * i + 1];
        if (y < *y_top) {
            *top = i;
            *y_top = y;
        } else if (*y_bottom < y) {
            *bottom = i;
            *y_bottom = y;
        }
    }
}

/* Winding: the quad is walked so that the edge with the smaller x slope is on
 * the left. Returns the corner step for the left chain. */
static int choose_direction(const int32_t screen[8], int top)
{
    const int32_t *p = screen + 2 * top;
    const int32_t *next = screen + 2 * ((top + 1) & 3), *prev = screen + 2 * ((top - 1) & 3);
    const int32_t s_next = x_div(x_shl(x_sub(next[0], p[0]), 16), x_add(x_sub(next[1], p[1]), 1));
    const int32_t s_prev = x_div(x_shl(x_sub(prev[0], p[0]), 16), x_add(x_sub(prev[1], p[1]), 1));
    return s_next < s_prev ? -1 : 1;
}

static void draw_quad(const QuadTarget *t, const FshEntry *texture, const int32_t uv[8],
                      const int32_t screen[8], int transparent)
{
    Edges edges;
    memset(&edges, 0, sizeof edges);
    const Texture tex = { texture, texture_row_bytes(texture) };
    Outline o = { t, screen, uv, &edges, 0, 0 };
    int32_t y_min, y_max;
    find_extremes(screen, &o.top_corner, &o.bottom_corner, &y_min, &y_max);
    if (y_min >= t->clip_bottom || y_max < t->clip_top) return;

    int step = choose_direction(screen, o.top_corner);
    trace_outline(&o, step);
    if (y_min < t->clip_top) y_min = t->clip_top;
    if (y_max >= t->clip_bottom) y_max = t->clip_bottom - 1;

    int flipped = 0;
    int32_t y = y_min;
    for (int32_t rows = y_max - y_min + 1; rows-- != 0;) {
        const int32_t width = x_add(x_sub(edges.x_right[y], edges.x_left[y]), 1);
        if (width < 1) {
            /* The tables disagree about which side is left: retrace once the
             * other way round, then look at this row again. A second failure
             * ends the draw. */
            if (flipped) return;
            flipped = 1;
            step = -step;
            trace_outline(&o, step);
            rows++;
            continue;
        }
        if (span_visible(t, &edges, y)) draw_row(t, &tex, &edges, y, width, transparent);
        y++;
    }
}

void quad_draw(const QuadTarget *target, const FshEntry *texture, const int32_t uv[8],
               const int32_t screen[8], int transparent)
{
    if (!target_valid(target) || !texture_valid(texture) || uv == NULL || screen == NULL) return;
    draw_quad(target, texture, uv, screen, transparent);
}

/* 00487a10 and 004879b0. */
void quad_draw_full(const QuadTarget *target, const FshEntry *texture, const int32_t screen[8],
                    int transparent)
{
    if (!texture_valid(texture)) return;
    const int32_t right = x_sub(x_shl((int16_t)texture->width, 16), 1);
    const int32_t bottom = x_sub(x_shl((int16_t)texture->height, 16), 1);
    const int32_t uv[8] = { 0, 0, right, 0, right, bottom, 0, bottom };
    quad_draw(target, texture, uv, screen, transparent);
}
