#include "viewer/raster.h"

#include <math.h>

enum { MAX_VERTS = 16 };

typedef struct {
    float sx, sy;      /* screen */
    float iz, uz, vz;  /* 1/z, u/z, v/z */
} Projected;

static RasterVertex lerp(RasterVertex a, RasterVertex b, float t)
{
    return (RasterVertex){ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
                           a.u + (b.u - a.u) * t, a.v + (b.v - a.v) * t };
}

/* Sutherland-Hodgman against z >= near. */
static int clip_near(const RasterVertex *in, int count, float near_z, RasterVertex *out)
{
    int n = 0;
    for (int i = 0; i < count; i++) {
        const RasterVertex a = in[i], b = in[(i + 1) % count];
        const int a_in = a.z >= near_z, b_in = b.z >= near_z;
        if (a_in) out[n++] = a;
        if (a_in != b_in) out[n++] = lerp(a, b, (near_z - a.z) / (b.z - a.z));
    }
    return n;
}

static uint8_t texel(const FshEntry *tex, float u, float v)
{
    int x = (int)floorf(u) % tex->width, y = (int)floorf(v) % tex->height;
    if (x < 0) x += tex->width;
    if (y < 0) y += tex->height;
    return tex->payload[(size_t)y * tex->width + x];
}

static float edge(const Projected *a, const Projected *b, float x, float y)
{
    return (b->sx - a->sx) * (y - a->sy) - (b->sy - a->sy) * (x - a->sx);
}

static void triangle(const RasterTarget *t, const Projected *p0, const Projected *p1, const Projected *p2,
                     const FshEntry *tex, int transparent, uint8_t colour)
{
    const float area = edge(p0, p1, p2->sx, p2->sy);
    if (fabsf(area) < 1e-6f) return;
    const int x0 = (int)fmaxf(0.0f, floorf(fminf(p0->sx, fminf(p1->sx, p2->sx))));
    const int x1 = (int)fminf((float)t->width - 1, ceilf(fmaxf(p0->sx, fmaxf(p1->sx, p2->sx))));
    const int y0 = (int)fmaxf(0.0f, floorf(fminf(p0->sy, fminf(p1->sy, p2->sy))));
    const int y1 = (int)fminf((float)t->height - 1, ceilf(fmaxf(p0->sy, fmaxf(p1->sy, p2->sy))));
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            const float w0 = edge(p1, p2, px, py) / area, w1 = edge(p2, p0, px, py) / area;
            const float w2 = 1.0f - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            uint8_t c = colour;
            if (tex != NULL) {
                const float iz = w0 * p0->iz + w1 * p1->iz + w2 * p2->iz;
                c = texel(tex, (w0 * p0->uz + w1 * p1->uz + w2 * p2->uz) / iz,
                          (w0 * p0->vz + w1 * p1->vz + w2 * p2->vz) / iz);
                if (transparent && c == 0xFF) continue;
            }
            t->pixels[y * t->width + x] = c;
        }
    }
}

void raster_polygon(const RasterTarget *t, const RasterVertex *in, int count, const FshEntry *texture,
                    int transparent, uint8_t colour)
{
    RasterVertex clipped[MAX_VERTS];
    if (count < 3 || count > 8) return;
    const int n = clip_near(in, count, t->near_z, clipped);
    if (n < 3) return;
    Projected p[MAX_VERTS];
    for (int i = 0; i < n; i++) {
        const float iz = 1.0f / clipped[i].z;
        p[i] = (Projected){ t->width * 0.5f + clipped[i].x * t->focal * iz,
                            t->height * 0.5f - clipped[i].y * t->focal * iz, iz, clipped[i].u * iz,
                            clipped[i].v * iz };
    }
    for (int i = 1; i + 1 < n; i++) triangle(t, &p[0], &p[i], &p[i + 1], texture, transparent, colour);
}

static void screen_triangle(const RasterTarget *t, const float *x, const float *y, const float *u, const float *v,
                            const FshEntry *tex, int transparent)
{
    const float area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
    if (fabsf(area) < 1e-6f) return;
    const int x0 = (int)fmaxf(0.0f, floorf(fminf(x[0], fminf(x[1], x[2]))));
    const int x1 = (int)fminf((float)t->width - 1, ceilf(fmaxf(x[0], fmaxf(x[1], x[2]))));
    const int y0 = (int)fmaxf(0.0f, floorf(fminf(y[0], fminf(y[1], y[2]))));
    const int y1 = (int)fminf((float)t->height - 1, ceilf(fmaxf(y[0], fmaxf(y[1], y[2]))));
    for (int py = y0; py <= y1; py++) {
        for (int px = x0; px <= x1; px++) {
            const float cx = px + 0.5f, cy = py + 0.5f;
            const float w1 = ((x[2] - x[0]) * (cy - y[0]) - (y[2] - y[0]) * (cx - x[0])) / -area;
            const float w2 = ((x[1] - x[0]) * (cy - y[0]) - (y[1] - y[0]) * (cx - x[0])) / area;
            const float w0 = 1.0f - w1 - w2;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            const uint8_t c = texel(tex, w0 * u[0] + w1 * u[1] + w2 * u[2], w0 * v[0] + w1 * v[1] + w2 * v[2]);
            if (transparent && c == 0xFF) continue;
            t->pixels[py * t->width + px] = c;
        }
    }
}

void raster_screen_quad(const RasterTarget *t, const int32_t screen[8], const int32_t uv[8], const FshEntry *texture,
                        int transparent)
{
    for (int i = 0; i < 8; i++)
        if (screen[i] < -100000 || screen[i] > 100000) return;  /* behind the eye */
    float x[4], y[4], u[4], v[4];
    for (int i = 0; i < 4; i++) {
        x[i] = (float)screen[2 * i], y[i] = (float)screen[2 * i + 1];
        u[i] = uv[2 * i] / 65536.0f, v[i] = uv[2 * i + 1] / 65536.0f;
    }
    const float xa[3] = { x[0], x[1], x[2] }, ya[3] = { y[0], y[1], y[2] }, ua[3] = { u[0], u[1], u[2] },
                va[3] = { v[0], v[1], v[2] };
    const float xb[3] = { x[0], x[2], x[3] }, yb[3] = { y[0], y[2], y[3] }, ub[3] = { u[0], u[2], u[3] },
                vb[3] = { v[0], v[2], v[3] };
    screen_triangle(t, xa, ya, ua, va, texture, transparent);
    screen_triangle(t, xb, yb, ub, vb, texture, transparent);
}

void raster_screen_triangle(const RasterTarget *t, const int32_t screen[6], const int32_t uv[6], const FshEntry *texture,
                            int transparent)
{
    float x[3], y[3], u[3], v[3];
    for (int i = 0; i < 3; i++) {
        if (screen[2 * i] < -100000 || screen[2 * i] > 100000 || screen[2 * i + 1] < -100000 || screen[2 * i + 1] > 100000)
            return;  /* behind the eye */
        x[i] = (float)screen[2 * i], y[i] = (float)screen[2 * i + 1];
        u[i] = (float)uv[2 * i], v[i] = (float)uv[2 * i + 1];
    }
    screen_triangle(t, x, y, u, v, texture, transparent);
}

void raster_fill(const RasterTarget *t, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t colour)
{
    int64_t x1 = (int64_t)x + w, y1 = (int64_t)y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > t->width) x1 = t->width;
    if (y1 > t->height) y1 = t->height;
    for (int64_t row = y; row < y1; row++)
        for (int64_t col = x; col < x1; col++) t->pixels[row * t->width + col] = colour;
}
