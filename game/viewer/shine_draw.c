/* port (body shine): see shine_draw.h. */
#include "viewer/shine_draw.h"

#include "game/gfx/raster_record.h"
#include "game/race/car_envmap.h"
#include "game/race/car_repaint.h"

#include <math.h>
#include <string.h>

/* the class map's codes are the draft's (car_repaint.h): paint, matte and shine (glass, lamps) */
enum { MAX_VERTS = 8, SHINE_PAINT = REPAINT_DRAFT_PAINT, SHINE_GLASS = REPAINT_DRAFT_SHINE, SHINE_BODY = REPAINT_DRAFT_BODY };
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

/* what a pass draws with: the envmap's tables (the paint's or the glass's, per texel) and image */
typedef struct {
    const ShineClass *cls;
    const EnvShine *env;
} Shine;

/* a corner on the screen: 1 / z (1 for the affine pass), u, v and the envmap's sphere uv, each over z */
typedef struct {
    float sx, sy, iz, uz, vz, eu, ev;
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

/* port (body_shine 4): the pixel at (x, y) takes the envmap's level at its interpolated uv, at the paint's intensity or the glass's, dithered,
 * through the paint table or the glass table */
static void envmap_pixel(uint8_t *px, const EnvShine *env, int glass, float eu, float ev, int x, int y)
{
    const uint8_t sample = car_envmap_sample(env->image, eu, ev);
    const float gain = glass ? CAR_ENVMAP_GLASS_GAIN : CAR_ENVMAP_BODY_GAIN;
    const float want = car_envmap_level(sample, gain);
    const int lo = (int)floorf(want);
    /* between two levels the entries may be far apart in colour (a red palette steps from red to pink): a dither between them is noise, so the
     * pixel takes the nearer one there (car_envmap_hard) */
    const uint8_t (*hard)[256] = glass ? env->glass_hard : env->paint_hard;
    const float threshold = hard != NULL && lo >= 0 && lo < CAR_SHINE_LEVELS && hard[lo][*px] ? 0.5f : BAYER[y & 3][x & 3];
    int level = (int)floorf(want + threshold);
    level = level < 0 ? 0 : level >= CAR_SHINE_LEVELS ? CAR_SHINE_LEVELS - 1 : level;
    *px = (glass ? env->glass : env->paint)[level][*px];
}

/* The mask's class of the texel (u, v), clamped to the class texture's edges. */
static uint8_t texel_class(const Shine *sh, float u, float v)
{
    int tx = (int)floorf(u), ty = (int)floorf(v);
    tx = tx < 0 ? 0 : tx >= sh->cls->width ? sh->cls->width - 1 : tx;
    ty = ty < 0 ? 0 : ty >= sh->cls->height ? sh->cls->height - 1 : ty;
    return sh->cls->klass[(size_t)ty * (size_t)sh->cls->width + (size_t)tx];
}

static void triangle(const QuadTarget *q, const Screen *a, const Screen *b, const Screen *c, Outline o, float origin, float grow,
                     float row_reach, const Shine *sh)
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
            const uint8_t k = texel_class(sh, u, v);
            /* the mask drives the shine: paint (0) at the body intensity, tinted by the paint; glass and lamps (128) at the glass
             * intensity, keeping their own colour; a body texel (192) keeps its colour too but takes the paint's intensity and table;
             * matte (255) and the rest not at all */
            if (k != SHINE_PAINT && k != SHINE_GLASS && k != SHINE_BODY) continue;
            const float eu = (wa * a->eu + wb * b->eu + wc * c->eu) * r, ev = (wa * a->ev + wb * b->ev + wc * c->ev) * r;
            envmap_pixel(&row[x], sh->env, k == SHINE_GLASS, eu, ev, x, y);
        }
    }
}

static void fan(const QuadTarget *q, const Screen *s, int n, float origin, float grow, float row_reach, const Shine *sh)
{
    for (int i = 1; i + 1 < n; i++) triangle(q, &s[0], &s[i], &s[i + 1], (Outline){ i == 1, 1, i + 2 == n }, origin, grow, row_reach, sh);
}

/* `eo` is the envmap's uv per corner, 2 floats each */
static void draw_affine(const QuadTarget *q, const Shine *sh, const int32_t screen[8], const int32_t uv[8], const float eo[8], int corners)
{
    Screen s[4];
    for (int i = 0; i < corners; i++)
        s[i] = (Screen){ (float)screen[2 * i], (float)screen[2 * i + 1], 1.0f, (float)uv[2 * i], (float)uv[2 * i + 1], eo[2 * i], eo[2 * i + 1] };
    fan(q, s, corners, 0.0f, 0.0f, ROW_REACH, sh);
}

typedef struct {
    float x, y, z, u, v, eu, ev;
} Vert;

static Vert lerp(Vert a, Vert b, float t)
{
    return (Vert){ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.u + (b.u - a.u) * t,
                   a.v + (b.v - a.v) * t, a.eu + (b.eu - a.eu) * t, a.ev + (b.ev - a.ev) * t };
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

static void draw_camera(const QuadTarget *q, const Shine *sh, const int32_t uv[8], const int32_t *const cam[4], const float eo[8], int corners)
{
    if (kept_view == NULL) return;
    Vert in[4], out[MAX_VERTS];
    for (int i = 0; i < corners; i++)
        in[i] = (Vert){ (float)cam[i][0], (float)cam[i][1], (float)cam[i][2], uv[2 * i] / 65536.0f, uv[2 * i + 1] / 65536.0f, eo[2 * i],
                        eo[2 * i + 1] };
    const int n = clip_near(in, corners, (float)kept_view->near_z, out);
    if (n < 3) return;
    Screen s[MAX_VERTS];
    const float focal = (float)kept_view->focal_x;   /* the exe scales both axes by focal_x's (modern_road.c) */
    for (int i = 0; i < n; i++) {
        const float iz = 1.0f / out[i].z;
        s[i] = (Screen){ kept_view->centre_x + out[i].x * focal * iz, kept_view->centre_y - out[i].y * focal * iz, iz, out[i].u * iz,
                         out[i].v * iz, out[i].eu * iz, out[i].ev * iz };
    }
    fan(q, s, n, 0.5f, EDGE_GROW, 0.0f, sh);
}

enum { KIND_AFFINE = 0, KIND_CAMERA = 1 };

static void unpack_corners(const int32_t packed[4], int corners, float eo[8])
{
    for (int i = 0; i < corners; i++) car_envmap_unpack(packed[i], eo + 2 * i);
}

/* the recorded forms: affine keeps screen (8), uv (8) and the packed envmap uvs (4) in c->v; camera keeps uv (8), the corners (12) and the packed
 * uvs (4); c->shape is the EnvShine */
static void replay(const RasterCmd *c, const QuadTarget *whole, int32_t top, int32_t bottom)
{
    QuadTarget band;
    if (!raster_narrow(whole, top, bottom, &band)) return;
    const Shine sh = { c->table, (const EnvShine *)c->shape };
    float eo[8];
    if (c->mode == KIND_AFFINE) {
        unpack_corners(c->v + 16, c->count, eo);
        draw_affine(&band, &sh, c->v, c->v + 8, eo, c->count);
    } else {
        const int32_t *cam[4] = { c->v + 8, c->v + 11, c->v + 14, c->v + 17 };
        unpack_corners(c->v + 20, c->count, eo);
        draw_camera(&band, &sh, c->v, cam, eo, c->count);
    }
}

static int usable(const ShineClass *cls, int corners)
{
    return cls != NULL && cls->klass != NULL && cls->width > 0 && cls->height > 0 && (corners == 3 || corners == 4);
}

void shine_draw_env_affine(const QuadTarget *q, const ShineClass *cls, const EnvShine *env, const int32_t screen[8], const int32_t uv[8],
                           const int32_t packed[4], int corners)
{
    if (!usable(cls, corners) || env == NULL || env->image == NULL) return;
    struct RasterRecorder *rec = raster_recorder_for(q);
    if (rec != NULL) {
        RasterCmd *c = raster_record_add(rec, q, replay);
        c->mode = KIND_AFFINE, c->count = corners, c->table = cls, c->shape = (const uint8_t *)env;
        memcpy(c->v, screen, 8 * sizeof *screen);
        memcpy(c->v + 8, uv, 8 * sizeof *uv);
        memcpy(c->v + 16, packed, 4 * sizeof *packed);
        raster_cmd_rows(c, screen, corners);
        return;
    }
    float eo[8];
    unpack_corners(packed, corners, eo);
    const Shine sh = { cls, env };
    draw_affine(q, &sh, screen, uv, eo, corners);
}

void shine_draw_env_camera(const QuadTarget *q, const ShineClass *cls, const EnvShine *env, const int32_t uv[8], const int32_t *const cam[4],
                           const int32_t packed[4], int corners)
{
    if (!usable(cls, corners) || env == NULL || env->image == NULL) return;
    struct RasterRecorder *rec = raster_recorder_for(q);
    if (rec != NULL) {
        RasterCmd *c = raster_record_add(rec, q, replay);
        c->mode = KIND_CAMERA, c->count = corners, c->table = cls, c->shape = (const uint8_t *)env;
        memcpy(c->v, uv, 8 * sizeof *uv);
        for (int i = 0; i < corners; i++) memcpy(c->v + 8 + 3 * i, cam[i], 3 * sizeof *cam[i]);
        memcpy(c->v + 20, packed, 4 * sizeof *packed);
        return;
    }
    float eo[8];
    unpack_corners(packed, corners, eo);
    const Shine sh = { cls, env };
    draw_camera(q, &sh, uv, cam, eo, corners);
}
