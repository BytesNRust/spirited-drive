/* port (body shine): see shine_draw.h. */
#include "viewer/shine_draw.h"

#include "game/gfx/raster_record.h"

#include <math.h>
#include <string.h>

enum { MAX_VERTS = 8 };
static const float EDGE_GROW = 0.45f;   /* modern_road.c's: a polygon's outer edges reach this much of a pixel out */
static const float ROW_REACH = 0.5f;    /* the exe's mapper rounds a row's ends: half a pixel past an outer edge, either way (the square the mapper's rounding spans) */
static const float IZ_MIN = 1e-9f;

/* the 4x4 ordered dither's thresholds, as modern_road.c's: (BAYER + 0.5) / 16 by (y & 3, x & 3) */
static const float BAYER[4][4] = {
    { 0.5f / 16, 8.5f / 16, 2.5f / 16, 10.5f / 16 },
    { 12.5f / 16, 4.5f / 16, 14.5f / 16, 6.5f / 16 },
    { 3.5f / 16, 11.5f / 16, 1.5f / 16, 9.5f / 16 },
    { 15.5f / 16, 7.5f / 16, 13.5f / 16, 5.5f / 16 },
};

static const View *kept_view;

void shine_draw_set_view(const View *view) { kept_view = view; }

/* a corner on the screen: 1 / z (1 for the affine pass) and u, v, level over z */
typedef struct {
    float sx, sy, iz, uz, vz, lz;
} Screen;

static float edge(const Screen *a, const Screen *b, float x, float y)
{
    return (b->sx - a->sx) * (y - a->sy) - (b->sy - a->sy) * (x - a->sx);
}

/* The pass follows each polygon's own exe draw, which overwrites whatever a neighbour drew there, so it reaches exactly the pixels that draw
 * fills: the closed outline, a pixel on an edge included (a shared edge is drawn by both polygons, the later over the earlier, and each
 * pass follows its own draw). One pixel more is a pixel the polygon did not redraw, shined twice (a light line down a seam); one less is a dark
 * dot. Only the diagonals inside one polygon's fan are exact: a pixel on one belongs to the side the edge's direction names, as the fan's two
 * triangles are one draw. `origin` is where a pixel sits in the coordinates: the exe's affine mapper has no centre offset (0), the 3D one
 * (modern_road.c) takes pixel centres (0.5) and grows a polygon's outer edges by EDGE_GROW of a pixel (`grow`), as this does. */
typedef struct {
    int outer_ab, outer_bc, outer_ca;
} Outline;

/* the least edge value a pixel may have: nothing for an inside diagonal, `grow` pixels past an outer edge (the edge value is the distance times its length) */
static float reach(const Screen *from, const Screen *to, int outer, float grow, float row_reach)
{
    return outer ? -grow * hypotf(to->sx - from->sx, to->sy - from->sy) - row_reach * (fabsf(to->sy - from->sy) + fabsf(to->sx - from->sx)) : 0.0f;
}

static int owns_tie(const Screen *from, const Screen *to)
{
    const float dx = to->sx - from->sx, dy = to->sy - from->sy;
    return dy > 0 || (dy == 0 && dx < 0);
}

static void triangle(const QuadTarget *q, const Screen *a, const Screen *b, const Screen *c, Outline o, float origin,
                     float grow, float row_reach, const ShineClass *cls, ShineTables tables)
{
    const float area = edge(a, b, c->sx, c->sy);
    if (fabsf(area) < 1e-6f) return;
    if (area < 0) {   /* wind it one way, so the tie rule means the same to every polygon */
        const Screen *t = b;
        b = c, c = t;
        const int u = o.outer_ab;
        o.outer_ab = o.outer_ca, o.outer_ca = u;
    }
    const float inv = 1.0f / fabsf(area);
    const float min_bc = reach(b, c, o.outer_bc, grow, row_reach), min_ca = reach(c, a, o.outer_ca, grow, row_reach),
                min_ab = reach(a, b, o.outer_ab, grow, row_reach);
    const int own_bc = o.outer_bc || owns_tie(b, c), own_ca = o.outer_ca || owns_tie(c, a), own_ab = o.outer_ab || owns_tie(a, b);
    const int x0 = (int)fmaxf((float)q->clip_left, floorf(fminf(a->sx, fminf(b->sx, c->sx))) - 1),
              x1 = (int)fminf((float)q->clip_right - 1, ceilf(fmaxf(a->sx, fmaxf(b->sx, c->sx))) + 1);
    const int y0 = (int)fmaxf((float)q->clip_top, floorf(fminf(a->sy, fminf(b->sy, c->sy))) - 1),
              y1 = (int)fminf((float)q->clip_bottom - 1, ceilf(fmaxf(a->sy, fmaxf(b->sy, c->sy))) + 1);
    for (int y = y0; y <= y1; y++) {
        uint8_t *row = q->pixels + (size_t)y * (size_t)q->pitch;
        const float py = y + origin;
        for (int x = x0; x <= x1; x++) {
            const float px = x + origin;
            const float ea = edge(b, c, px, py), eb = edge(c, a, px, py), ec = edge(a, b, px, py);
            if (ea < min_bc || eb < min_ca || ec < min_ab || (ea == 0 && !own_bc) || (eb == 0 && !own_ca) || (ec == 0 && !own_ab)) continue;
            const float wa = ea * inv, wb = eb * inv, wc = ec * inv;
            const float iz = wa * a->iz + wb * b->iz + wc * c->iz;
            if (iz < IZ_MIN) continue;
            const float r = 1.0f / iz;
            const float u = (wa * a->uz + wb * b->uz + wc * c->uz) * r, v = (wa * a->vz + wb * b->vz + wc * c->vz) * r;
            int tx = (int)floorf(u), ty = (int)floorf(v);
            tx = tx < 0 ? 0 : tx >= cls->width ? cls->width - 1 : tx;
            ty = ty < 0 ? 0 : ty >= cls->height ? cls->height - 1 : ty;
            if (cls->klass[(size_t)ty * (size_t)cls->width + (size_t)tx] != 0) continue;   /* not paint */
            const float l = (wa * a->lz + wb * b->lz + wc * c->lz) * r;
            int level = (int)floorf(l + BAYER[y & 3][x & 3]);
            level = level < 0 ? 0 : level >= CAR_SHINE_LEVELS ? CAR_SHINE_LEVELS - 1 : level;
            row[x] = tables[level][row[x]];
        }
    }
}

static void fan(const QuadTarget *q, const Screen *s, int n, float origin, float grow, float row_reach, const ShineClass *cls, ShineTables tables)
{
    for (int i = 1; i + 1 < n; i++) triangle(q, &s[0], &s[i], &s[i + 1], (Outline){ i == 1, 1, i + 2 == n }, origin, grow, row_reach, cls, tables);
}

static void draw_affine(const QuadTarget *q, const ShineClass *cls, ShineTables tables, const int32_t screen[8], const int32_t uv[8],
                        const float level[4], int corners)
{
    Screen s[4];
    for (int i = 0; i < corners; i++)
        s[i] = (Screen){ (float)screen[2 * i], (float)screen[2 * i + 1], 1.0f, (float)uv[2 * i], (float)uv[2 * i + 1], level[i] };
    fan(q, s, corners, 0.0f, 0.0f, ROW_REACH, cls, tables);
}

typedef struct {
    float x, y, z, u, v, l;
} Vert;

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

static void draw_camera(const QuadTarget *q, const ShineClass *cls, ShineTables tables, const int32_t uv[8], const int32_t *const cam[4],
                        const float level[4], int corners)
{
    if (kept_view == NULL) return;
    Vert in[4], out[MAX_VERTS];
    for (int i = 0; i < corners; i++)
        in[i] = (Vert){ (float)cam[i][0], (float)cam[i][1], (float)cam[i][2], uv[2 * i] / 65536.0f, uv[2 * i + 1] / 65536.0f, level[i] };
    const int n = clip_near(in, corners, (float)kept_view->near_z, out);
    if (n < 3) return;
    Screen s[MAX_VERTS];
    const float focal = (float)kept_view->focal_x;   /* the exe scales both axes by focal_x's (modern_road.c) */
    for (int i = 0; i < n; i++) {
        const float iz = 1.0f / out[i].z;
        s[i] = (Screen){ kept_view->centre_x + out[i].x * focal * iz, kept_view->centre_y - out[i].y * focal * iz, iz, out[i].u * iz,
                         out[i].v * iz, out[i].l * iz };
    }
    fan(q, s, n, 0.5f, EDGE_GROW, 0.0f, cls, tables);
}

enum { KIND_AFFINE = 0, KIND_CAMERA = 1 };

/* the recorded forms: affine keeps screen (8), uv (8) and the levels (4 floats) in c->v; camera keeps uv (8), the corners (12) and the levels */
static void replay(const RasterCmd *c, const QuadTarget *whole, int32_t top, int32_t bottom)
{
    QuadTarget band;
    if (!raster_narrow(whole, top, bottom, &band)) return;
    const ShineClass *cls = c->table;
    ShineTables tables = (ShineTables)c->shape;
    float level[4];
    if (c->mode == KIND_AFFINE) {
        memcpy(level, c->v + 16, sizeof level);
        draw_affine(&band, cls, tables, c->v, c->v + 8, level, c->count);
    } else {
        const int32_t *cam[4] = { c->v + 8, c->v + 11, c->v + 14, c->v + 17 };
        memcpy(level, c->v + 20, sizeof level);
        draw_camera(&band, cls, tables, c->v, cam, level, c->count);
    }
}

void shine_draw_affine(const QuadTarget *q, const ShineClass *cls, ShineTables tables, const int32_t screen[8], const int32_t uv[8],
                       const float level[4], int corners)
{
    if (cls == NULL || cls->klass == NULL || cls->width <= 0 || cls->height <= 0 || (corners != 3 && corners != 4)) return;
    struct RasterRecorder *rec = raster_recorder_for(q);
    if (rec != NULL) {
        RasterCmd *c = raster_record_add(rec, q, replay);
        c->mode = KIND_AFFINE, c->count = corners, c->table = cls, c->shape = (const uint8_t *)tables;
        memcpy(c->v, screen, 8 * sizeof *screen);
        memcpy(c->v + 8, uv, 8 * sizeof *uv);
        memcpy(c->v + 16, level, 4 * sizeof *level);
        raster_cmd_rows(c, screen, corners);
        return;
    }
    draw_affine(q, cls, tables, screen, uv, level, corners);
}

void shine_draw_camera(const QuadTarget *q, const ShineClass *cls, ShineTables tables, const int32_t uv[8], const int32_t *const cam[4],
                       const float level[4], int corners)
{
    if (cls == NULL || cls->klass == NULL || cls->width <= 0 || cls->height <= 0 || (corners != 3 && corners != 4)) return;
    struct RasterRecorder *rec = raster_recorder_for(q);
    if (rec != NULL) {
        RasterCmd *c = raster_record_add(rec, q, replay);
        c->mode = KIND_CAMERA, c->count = corners, c->table = cls, c->shape = (const uint8_t *)tables;
        memcpy(c->v, uv, 8 * sizeof *uv);
        for (int i = 0; i < corners; i++) memcpy(c->v + 8 + 3 * i, cam[i], 3 * sizeof *cam[i]);
        memcpy(c->v + 20, level, 4 * sizeof *level);
        return;
    }
    draw_camera(q, cls, tables, uv, cam, level, corners);
}
