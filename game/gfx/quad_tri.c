/* Triangle mapper and the two-triangle whole-texture quad built on it.
 * Ported from NFS_WIN.EXE 004859f8 (opaque) and 00484ee8 (colour 0xFF
 * transparent), their 16.16 wrappers 00486400 and 004858f0, and the callers
 * 00486720 and 004864f0 that split an inset texture into two triangles.
 *
 * Unlike the quad mapper this one works in 16.16 throughout, with pixel
 * centres at +0.5, and fills x from floor(left) up to but excluding
 * floor(right). It follows only the left edge's texture position and applies a
 * constant (du, dx), (dv, dx) per pixel through a table of texel offsets
 * (0049f580) that it builds once per triangle.
 *
 * port: 00484ee8 has a no-clip fast path chosen by comparing
 * texture coordinates and 16.16 screen x against the pixel clip limits; with
 * uv >= 1 texel and clip_right <= 32768 that test never passes, so it is not
 * ported (the clipped path gives the same pixels when nothing needs clipping). */
#include "game/gfx/quad.h"

#include "game/gfx/quad_pixel.h"
#include "game/gfx/x86_int.h"

#include <stddef.h>

enum {
    TABLE_SIZE = 0x600,        /* offsets built per triangle: widest span 0x5FF pixels + 1 */
    MIN_WIDTH = 0xFFFF,        /* narrowest triangle drawn: one pixel, exclusive */
    MAX_WIDTH = 0x6000000,     /* widest triangle drawn: 1536 pixels, exclusive */
    HALF = 0x8000,
    FSH_TYPE_BITMAP_TRANSPARENT = 0x45,
    MAX_CLIP_RIGHT = 0x8000
};

typedef struct {
    int32_t x, y;
} Point;

typedef struct {
    const QuadTarget *target;
    const FshEntry *texture;
    Paint paint;
    int32_t width;                 /* texture width, signed as the game reads it */
    int32_t offsets[TABLE_SIZE];   /* DAT_0052d3a8, zero where not built */
} Mapper;

typedef struct {
    int32_t left, right;  /* x, 16.16 */
    int32_t u, v;         /* texture position on the left edge */
} Cursor;

typedef struct {
    int32_t left, right, u, v;  /* change per row */
} Steps;

static int32_t divide_fixed(int32_t a, int32_t b)
{
    return b != 0 ? x_div64((int64_t)a * 65536, b) : a;
}

static uint8_t texel_at(const FshEntry *e, int64_t offset)
{
    return offset >= 0 && (uint64_t)offset < e->payload_size ? e->payload[offset] : 0;
}

static int32_t offset_at(const Mapper *m, int32_t index)
{
    return index >= 0 && index < TABLE_SIZE ? m->offsets[index] : 0;
}

/* 0049f580: offsets of the texels along a row, two per step. */
static void build_offsets(Mapper *m, int32_t du, int32_t dv, int32_t count)
{
    const uint32_t width = (uint32_t)m->width;
    int shift = 0;
    for (uint32_t w = width; w > 1; w >>= 1) shift++;
    const int power_of_two = (width & (width - 1)) == 0;
    int32_t u = HALF, v = HALF;
    for (int32_t i = 0; i < ((count + 1) >> 1) && 2 * i + 1 < TABLE_SIZE; i++) {
        const int32_t u1 = x_sar(x_add(u, du), 16), v1 = x_sar(x_add(v, dv), 16);
        if (power_of_two) {
            m->offsets[2 * i] = x_add(x_shl(x_sar(v, 16), shift), x_sar(u, 16));
            m->offsets[2 * i + 1] = x_add(x_shl(v1, shift), u1);
        } else {
            m->offsets[2 * i] = x_add(x_sar(u, 16), x_mul(x_sar(v, 16), m->width));
            m->offsets[2 * i + 1] = x_add(x_mul(v1, m->width), u1);
        }
        u = x_add(u, x_add(du, du));
        v = x_add(v, x_add(dv, dv));
    }
}

/* 0048cc49, 0048c89a and 0048ca53: count pixels, each texel at start + offsets[i]. */
static void blit(const Mapper *m, uint8_t *dst, int32_t start, int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        paint_texel(&m->paint, dst + i, texel_at(m->texture, x_add(start, offset_at(m, i))));
    }
}

/* One scanline of the clipped path of 00484ee8. */
static void draw_row(const Mapper *m, int32_t row, const Cursor *c)
{
    const QuadTarget *t = m->target;
    int32_t x = x_sar(c->left, 16);
    int32_t count = x_sub(x_sar(c->right, 16), x);
    const int32_t over = x_sub(x_add(x, count), t->clip_right);
    if (over > 0) count = x_sub(count, over);
    int32_t skipped = x_sub(t->clip_left, x);
    if (skipped < 1) {
        skipped = 0;
    } else {
        x = x_add(x, skipped);
        count = x_sub(count, skipped);
    }
    if (count > 0 && t->clip_top <= row) {
        const int32_t start = x_add(x_add(x_sar(c->u, 16), x_mul(m->width, x_sar(c->v, 16))),
                                    offset_at(m, skipped));
        blit(m, t->pixels + (size_t)row * (size_t)t->pitch + x, start, count);
    }
}

/* Draws rows [row, limit) and returns the first row not drawn. */
static int32_t run_rows(const Mapper *m, int32_t row, int32_t limit, Cursor *c, const Steps *s)
{
    for (; row < limit; row++) {
        draw_row(m, row, c);
        c->left = x_add(c->left, s->left);
        c->right = x_add(c->right, s->right);
        c->u = x_add(c->u, s->u);
        c->v = x_add(c->v, s->v);
    }
    return row;
}

typedef struct {
    Point top, mid, bottom;
    Point uv_top, uv_mid, uv_bottom;
    int32_t long_dx;       /* x step per row along top to bottom */
    int mid_is_left;
} Shape;

/* The row index (y >> 16) of a point, capped to the clip bottom. */
static int32_t row_limit(const Mapper *m, int32_t y)
{
    const int32_t row = x_sar(y, 16);
    return row > m->target->clip_bottom ? m->target->clip_bottom : row;
}

static void draw_upper(Mapper *m, const Shape *s, Cursor *c, Steps *st, int32_t *row)
{
    const int32_t dy = x_sub(s->top.y, s->mid.y);
    const int32_t short_dx = divide_fixed(x_sub(s->top.x, s->mid.x), dy);
    *st = (Steps){ 0 };
    if (s->mid_is_left) {
        st->left = short_dx;
        st->right = s->long_dx;
        st->u = divide_fixed(x_sub(s->uv_top.x, s->uv_mid.x), dy);
        st->v = divide_fixed(x_sub(s->uv_top.y, s->uv_mid.y), dy);
    } else {
        st->right = short_dx;
        st->left = s->long_dx;
        const int32_t dy_long = x_sub(s->top.y, s->bottom.y);
        st->u = divide_fixed(x_sub(s->uv_top.x, s->uv_bottom.x), dy_long);
        st->v = divide_fixed(x_sub(s->uv_top.y, s->uv_bottom.y), dy_long);
    }
    if (*row < x_sar(s->mid.y, 16)) *row = run_rows(m, *row, row_limit(m, s->mid.y), c, st);
}

static void draw_lower(Mapper *m, const Shape *s, Cursor *c, Steps *st, int32_t row)
{
    if (row >= x_sar(s->bottom.y, 16)) return;
    const int32_t dy = x_sub(s->mid.y, s->bottom.y);
    const int32_t short_dx = divide_fixed(x_sub(s->mid.x, s->bottom.x), dy);
    if (s->mid_is_left) {
        st->left = short_dx;
        st->right = s->long_dx;
        c->left = s->mid.x;
        c->u = s->uv_mid.x;
        c->v = s->uv_mid.y;
        st->u = divide_fixed(x_sub(s->uv_mid.x, s->uv_bottom.x), dy);
        st->v = divide_fixed(x_sub(s->uv_mid.y, s->uv_bottom.y), dy);
    } else {
        st->right = short_dx;
        c->right = s->mid.x;
        st->left = s->long_dx;
    }
    run_rows(m, row, row_limit(m, s->bottom.y), c, st);
}

/* Picks the top, bottom and middle vertices by y (the first lines of
 * 00484ee8); equal y values resolve towards the lower index. */
static void sort_vertices(const int32_t scr[6], int *top, int *bottom, int *mid)
{
    int lo = scr[3] < scr[1];
    int hi = scr[1] < scr[3];
    if (scr[2 * hi + 1] < scr[5]) hi = 2;
    if (scr[5] < scr[2 * lo + 1]) lo = 2;
    *top = lo;
    *bottom = hi;
    *mid = 3 - (lo | hi);
}

/* uv and scr are 3 x (x, y) in 16.16. */
static void draw_triangle(Mapper *m, const int32_t uv[6], const int32_t scr[6])
{
    int top, bottom, mid;
    sort_vertices(scr, &top, &bottom, &mid);
    /* All three on one row: mid would be 3, and dy is 0 below anyway. */
    if (top == bottom) return;
    const Shape s0 = {
        { scr[2 * top], scr[2 * top + 1] }, { scr[2 * mid], scr[2 * mid + 1] },
        { scr[2 * bottom], scr[2 * bottom + 1] },
        { uv[2 * top], uv[2 * top + 1] }, { uv[2 * mid], uv[2 * mid + 1] },
        { uv[2 * bottom], uv[2 * bottom + 1] }, 0, 0
    };
    const int32_t dx = x_sub(s0.bottom.x, s0.top.x), dy = x_sub(s0.bottom.y, s0.top.y);
    if (dy <= MIN_WIDTH) return;

    const int32_t mid_dy = x_sub(s0.mid.y, s0.top.y);
    const int32_t edge_x = x_add(s0.top.x, x_div64((int64_t)dx * mid_dy, dy));
    const int32_t span = x_sub(s0.mid.x, edge_x);
    const int32_t width = span < 0 ? x_sub(0, span) : span;
    if (!(width > MIN_WIDTH && width < MAX_WIDTH)) return;

    const int32_t below = x_sub(s0.bottom.y, s0.mid.y);
    const int32_t edge_u = x_add(s0.uv_bottom.x, x_div64((int64_t)x_sub(s0.uv_top.x, s0.uv_bottom.x) * below, dy));
    const int32_t edge_v = x_add(s0.uv_bottom.y, x_div64((int64_t)x_sub(s0.uv_top.y, s0.uv_bottom.y) * below, dy));
    build_offsets(m, divide_fixed(x_sub(s0.uv_mid.x, edge_u), span),
                  divide_fixed(x_sub(s0.uv_mid.y, edge_v), span), (width >> 16) + 1);

    Shape s = s0;
    s.long_dx = divide_fixed(dx, dy);
    s.mid_is_left = span < 0;
    Cursor c = { s0.top.x, s0.top.x, s0.uv_top.x, s0.uv_top.y };
    Steps steps;
    int32_t row = x_sar(s0.top.y, 16);
    draw_upper(m, &s, &c, &steps, &row);
    draw_lower(m, &s, &c, &steps, row);
}

/* 00486400 and 004858f0: integer pixel coordinates to 16.16 pixel centres. */
static void draw_triangle_pixels(Mapper *m, const int32_t uv[6], const int32_t scr[6])
{
    int32_t fuv[6], fscr[6];
    for (int i = 0; i < 6; i++) {
        fuv[i] = x_add(x_shl(uv[i], 16), HALF);
        fscr[i] = x_add(x_shl(scr[i], 16), HALF);
    }
    draw_triangle(m, fuv, fscr);
}

static int tri_inputs_valid(const QuadTarget *t, const FshEntry *e, const Paint *paint)
{
    return paint_valid(paint) && t != NULL && t->pixels != NULL && t->pitch > 0 && e != NULL && e->payload != NULL &&
           (e->type == FSH_TYPE_BITMAP || e->type == FSH_TYPE_BITMAP_TRANSPARENT) &&
           t->clip_left >= 0 && t->clip_left < t->clip_right && t->clip_right <= MAX_CLIP_RIGHT &&
           t->clip_top >= 0 && t->clip_top < t->clip_bottom && t->clip_bottom <= 768;
}

/* 00486400 (MAP_OPAQUE), 004858f0 (MAP_TRANSPARENT) and 004aa510 (MAP_SHADE): the
 * latter is 004a9b08, a copy of the transparent mapper 00484ee8 whose row
 * blitter 0048ca53 also handles colour 0xFE. */
void tri_draw_mode(const QuadTarget *target, const FshEntry *texture, const int32_t uv[6],
                   const int32_t screen[6], MapMode mode, const uint8_t *shade)
{
    const Paint paint = { mode, shade };
    if (!tri_inputs_valid(target, texture, &paint) || uv == NULL || screen == NULL) return;
    Mapper m = { .target = target, .texture = texture, .paint = paint,
                 .width = (int16_t)texture->width };
    draw_triangle_pixels(&m, uv, screen);
}

void tri_draw(const QuadTarget *target, const FshEntry *texture, const int32_t uv[6],
              const int32_t screen[6], int transparent)
{
    tri_draw_mode(target, texture, uv, screen, transparent ? MAP_TRANSPARENT : MAP_OPAQUE, NULL);
}

/* 00486720 and 004864f0. */
void quad_draw_full_tri(const QuadTarget *target, const FshEntry *texture, const int32_t screen[8],
                        int transparent)
{
    const Paint paint = { transparent ? MAP_TRANSPARENT : MAP_OPAQUE, NULL };
    if (!tri_inputs_valid(target, texture, &paint) || screen == NULL) return;
    Mapper m = { .target = target, .texture = texture, .paint = paint,
                 .width = (int16_t)texture->width };
    const int32_t right = (int32_t)(int16_t)texture->width - 2;
    const int32_t bottom = (int32_t)(int16_t)texture->height - 2;
    const int32_t uv_a[6] = { 1, 1, right, 1, right, bottom };
    draw_triangle_pixels(&m, uv_a, screen);
    const int32_t uv_b[6] = { right, bottom, 1, bottom, 1, 1 };
    const int32_t scr_b[6] = { screen[4], screen[5], screen[6], screen[7], screen[0], screen[1] };
    draw_triangle_pixels(&m, uv_b, scr_b);
}
