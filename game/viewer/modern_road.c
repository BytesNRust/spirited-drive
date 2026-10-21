/* port (spike): a near road cell drawn from its camera-space corners: clipped against the near plane rather than
 * pushed onto it, projected in floating point (no pixel snapping) and textured perspective-correct (no affine
 * warp). The exe's path is 00425718's subdivision into the affine mapper (track_draw.c, quad.c). */
#include "viewer/modern_road.h"

#include "game/gfx/quad_pixel.h"

#include <math.h>

enum { MAX_VERTS = 8 };

/* what a polygon is painted with: a texture through a MapMode's rules, or (texture NULL) one palette colour */
typedef struct {
    const FshEntry *texture;
    Paint paint;
    uint8_t colour;
} Fill;

typedef struct {
    float x, y, z, u, v;
} Vert;

typedef struct {
    float sx, sy, iz, uz, vz;
} Screen;

static Vert lerp(Vert a, Vert b, float t)
{
    return (Vert){ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.u + (b.u - a.u) * t,
                   a.v + (b.v - a.v) * t };
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

static uint8_t texel(const FshEntry *t, float u, float v)
{
    int x = (int)floorf(u), y = (int)floorf(v);
    x = x < 0 ? 0 : x >= t->width ? t->width - 1 : x;
    y = y < 0 ? 0 : y >= t->height ? t->height - 1 : y;
    return t->payload[(size_t)y * t->width + x];
}

static void triangle(const QuadTarget *q, const Screen *p0, const Screen *p1, const Screen *p2, const Fill *fill)
{
    const float area = edge(p0, p1, p2->sx, p2->sy);
    if (fabsf(area) < 1e-6f) return;
    const float lo_x = fminf(p0->sx, fminf(p1->sx, p2->sx)), hi_x = fmaxf(p0->sx, fmaxf(p1->sx, p2->sx));
    const float lo_y = fminf(p0->sy, fminf(p1->sy, p2->sy)), hi_y = fmaxf(p0->sy, fmaxf(p1->sy, p2->sy));
    const int x0 = (int)fmaxf((float)q->clip_left, floorf(lo_x)), x1 = (int)fminf((float)q->clip_right - 1, ceilf(hi_x));
    const int y0 = (int)fmaxf((float)q->clip_top, floorf(lo_y)), y1 = (int)fminf((float)q->clip_bottom - 1, ceilf(hi_y));
    for (int y = y0; y <= y1; y++) {
        uint8_t *row = q->pixels + (size_t)y * q->pitch;
        for (int x = x0; x <= x1; x++) {
            const float px = x + 0.5f, py = y + 0.5f;
            const float w0 = edge(p1, p2, px, py) / area, w1 = edge(p2, p0, px, py) / area, w2 = 1.0f - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            const float iz = w0 * p0->iz + w1 * p1->iz + w2 * p2->iz;
            if (fill->texture == NULL) {
                row[x] = fill->colour;
                continue;
            }
            const uint8_t c = texel(fill->texture, (w0 * p0->uz + w1 * p1->uz + w2 * p2->uz) / iz,
                                    (w0 * p0->vz + w1 * p1->vz + w2 * p2->vz) / iz);
            paint_texel(&fill->paint, &row[x], c);
        }
    }
}

static void draw_clipped(const QuadTarget *q, const View *view, const int32_t uv[8], const int32_t *const cam[4],
                         int corners, const Fill *fill)
{
    Vert in[4], out[MAX_VERTS];
    for (int i = 0; i < corners; i++)
        in[i] = (Vert){ (float)cam[i][0], (float)cam[i][1], (float)cam[i][2], uv[2 * i] / 65536.0f,
                        uv[2 * i + 1] / 65536.0f };
    const int n = clip_near(in, corners, (float)view->near_z, out);
    if (n < 3) return;
    Screen s[MAX_VERTS];
    const float focal = (float)view->focal_x;  /* 00475eac scales both axes by focal_x's reciprocals */
    for (int i = 0; i < n; i++) {
        const float iz = 1.0f / out[i].z;
        s[i] = (Screen){ view->centre_x + out[i].x * focal * iz, view->centre_y - out[i].y * focal * iz, iz,
                         out[i].u * iz, out[i].v * iz };
    }
    for (int i = 1; i + 1 < n; i++) triangle(q, &s[0], &s[i], &s[i + 1], fill);
}

void modern_road_cell(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8],
                      const int32_t *const cam[4], int transparent)
{
    const Fill fill = { texture, { transparent ? MAP_TRANSPARENT : MAP_OPAQUE, NULL }, 0 };
    draw_clipped(q, view, uv, cam, 4, &fill);
}

void modern_polygon(const QuadTarget *q, const View *view, const FshEntry *texture, const int32_t uv[8],
                    const int32_t *const cam[4], int corners, MapMode mode, const uint8_t *shade)
{
    if (texture == NULL || (corners != 3 && corners != 4)) return;
    const Fill fill = { texture, { mode, shade }, 0 };
    if (!paint_valid(&fill.paint)) return;
    draw_clipped(q, view, uv, cam, corners, &fill);
}

void modern_flat(const QuadTarget *q, const View *view, uint8_t colour, const int32_t *const cam[4], int corners)
{
    if (corners != 3 && corners != 4) return;
    const int32_t uv[8] = { 0 };
    const Fill fill = { NULL, { MAP_OPAQUE, NULL }, colour };
    draw_clipped(q, view, uv, cam, corners, &fill);
}
