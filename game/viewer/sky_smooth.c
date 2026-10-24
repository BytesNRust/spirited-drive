/* port: the sky ring's triangles blended (sky_smooth.h). */
#include "viewer/sky_smooth.h"

#include <math.h>

/* tri_draw's rule in whole-pixel coordinates (its +0.5 centres taken off): row y runs over [top, bottom) of the
 * corners' y, its edges taken at y, and a pixel x is drawn when left < x + 0.5 <= right. */
typedef struct {
    double x0, y0, x1, y1;   /* the end with the smaller y first (then the smaller x): the same numbers for both
                              * triangles that share the edge */
} Edge;

static Edge edge_make(const int32_t a[2], const int32_t b[2])
{
    const int a_first = a[1] < b[1] || (a[1] == b[1] && a[0] <= b[0]);
    const int32_t *p = a_first ? a : b, *q = a_first ? b : a;
    return (Edge){ p[0], p[1], q[0], q[1] };
}

static double edge_x(const Edge *e, double y)
{
    return e->y1 == e->y0 ? e->x0 : e->x0 + (e->x1 - e->x0) * (y - e->y0) / (e->y1 - e->y0);
}

/* the affine map of the texture over the triangle: u, v (16.16) at a pixel = base + dx * x + dy * y */
typedef struct {
    double base[2], dx[2], dy[2];
} Gradient;

static int gradient_make(const int32_t uv[6], const int32_t p[6], Gradient *g)
{
    const double ax = p[2] - p[0], ay = p[3] - p[1], bx = p[4] - p[0], by = p[5] - p[1];
    const double den = ax * by - bx * ay;
    if (den == 0.0) return 0;
    for (int k = 0; k < 2; k++) {
        const double da = (double)(uv[2 + k] - uv[k]) * 65536.0, db = (double)(uv[4 + k] - uv[k]) * 65536.0;
        g->dx[k] = (da * by - db * ay) / den;
        g->dy[k] = (db * ax - da * bx) / den;
        g->base[k] = uv[k] * 65536.0 - g->dx[k] * p[0] - g->dy[k] * p[1];
    }
    return 1;
}

typedef struct {
    const HorizonSmooth *s;
    const QuadTarget *t;
    const FshEntry *texture;
    Gradient g;
} Draw;

/* row y from x = left to right (exact edges), clipped */
static void draw_row(const Draw *d, int32_t y, double left, double right)
{
    const QuadTarget *t = d->t;
    double first = floor(left - 0.5) + 1.0, last = floor(right - 0.5);
    if (first < t->clip_left) first = t->clip_left;
    if (last > t->clip_right - 1) last = t->clip_right - 1;
    if (!(first <= last)) return;   /* also an edge far off the target, before its x is taken as an int */
    uint8_t *row = t->pixels + (size_t)y * (size_t)t->pitch;
    const double u[3] = { d->g.base[0], d->g.dx[0], d->g.dy[0] }, v[3] = { d->g.base[1], d->g.dx[1], d->g.dy[1] };
    horizon_smooth_span(d->s, d->texture, row, (int32_t)first, (int32_t)last, y, u, v);
}

static int target_valid(const QuadTarget *t, const FshEntry *e)
{
    return t != NULL && t->pixels != NULL && t->pitch > 0 && t->clip_left >= 0 && t->clip_left < t->clip_right &&
           t->clip_right <= t->pitch && t->clip_top >= 0 && t->clip_top < t->clip_bottom && e != NULL &&
           e->payload != NULL && e->width > 0 && e->height > 0 &&
           (size_t)e->width * (size_t)e->height <= e->payload_size;
}

void sky_smooth_triangle(const HorizonSmooth *s, const QuadTarget *target, const FshEntry *texture, const int32_t uv[6],
                         const int32_t screen[6])
{
    if (s == NULL || uv == NULL || screen == NULL || !target_valid(target, texture)) return;
    Draw d = { s, target, texture, { { 0, 0 }, { 0, 0 }, { 0, 0 } } };
    if (!gradient_make(uv, screen, &d.g)) return;
    /* the corners by y: top, mid, bottom */
    int top = 0, bottom = 0;
    for (int i = 1; i < 3; i++) {
        if (screen[2 * i + 1] < screen[2 * top + 1]) top = i;
        if (screen[2 * i + 1] >= screen[2 * bottom + 1]) bottom = i;
    }
    if (top == bottom) return;
    const int mid = 3 - top - bottom;
    const Edge lng = edge_make(&screen[2 * top], &screen[2 * bottom]);
    const Edge upper = edge_make(&screen[2 * top], &screen[2 * mid]);
    const Edge lower = edge_make(&screen[2 * mid], &screen[2 * bottom]);
    const int32_t mid_y = screen[2 * mid + 1];
    const int long_left = edge_x(&lng, mid_y) < screen[2 * mid];
    int32_t y = screen[2 * top + 1], end = screen[2 * bottom + 1];
    if (y < target->clip_top) y = target->clip_top;
    if (end > target->clip_bottom) end = target->clip_bottom;
    for (; y < end; y++) {
        const double a = edge_x(&lng, y), b = edge_x(y < mid_y ? &upper : &lower, y);
        draw_row(&d, y, long_left ? a : b, long_left ? b : a);
    }
}
