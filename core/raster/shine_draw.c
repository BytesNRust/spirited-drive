/* port (body shine): see shine_draw.h. */
#include "raster/shine_draw.h"

#include "formats/car_repaint.h"
#include "raster/car_envmap.h"
#include "raster/object_mappers.h"
#include "raster/raster_record.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* the class map's codes are the draft's (car_repaint.h): paint, matte and shine (glass, lamps) */
enum { MAX_VERTS = 8, SHINE_PAINT = REPAINT_DRAFT_PAINT, SHINE_GLASS = REPAINT_DRAFT_SHINE, SHINE_BODY = REPAINT_DRAFT_BODY };
static const float EDGE_GROW = 0.45f;   /* modern_road.c's: a polygon's outer edges reach this much of a pixel out */
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

/* Modern 3D's pass (draw_camera) follows modern_road.c's polygon: pixel centres (`origin` 0.5) and the outer edges grown by EDGE_GROW of a pixel
 * (`grow`), as that draw grows them, so the pass reaches the pixels its own draw just wrote. Only the diagonals inside one polygon's fan are exact:
 * a pixel on one belongs to the side the edge's direction names, as the fan's two triangles are one draw. The exe's path takes the mapper's own
 * pixels instead (mapper_cover). */
typedef struct {
    int outer_ab, outer_bc, outer_ca;
} Outline;

/* the least edge value a pixel may have: nothing for an inside diagonal, `grow` pixels past an outer edge (the edge value is the distance times its length) */
static float reach(const Screen *from, const Screen *to, int outer, float grow)
{
    return outer ? -grow * hypotf(to->sx - from->sx, to->sy - from->sy) : 0.0f;
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

/* port (body_shine, soft edges, prototype): the class map is one byte a texel, so where paint meets glass or the original colours the shine's edge falls on the texel's: a step the size of a
 * texel on a small texture drawn large. With the shine_soft setting (or TNFS_SHINE_SOFT=1) each pixel takes the weights of the shined classes (paint and body, glass: the draft's codes) bilinearly between the four texels nearest
 * its sub-texel place, and the ordered dither decides, from the same threshold as the levels', whether the pixel is shined at all (weight of the shined classes) and, if so, by which table
 * (paint first, then glass). A pixel whose four texels agree has weight 1 or 0: shined as before or not at all, byte for byte. Only which table a pixel takes changes, never which pixels are drawn. */
static int soft_setting;   /* the player's shine_soft, told by the game and the Studio (shine_draw_set_soft) */
static int render_scale;   /* the render scale the picture is drawn at (shine_draw_set_render_scale); 0: not told, no rule */
void shine_draw_set_soft(int on) { soft_setting = on != 0; }
void shine_draw_set_render_scale(int scale) { render_scale = scale; }
int shine_draw_get_soft(void) { return soft_setting; }
int shine_draw_get_render_scale(void) { return render_scale; }
static int soft_on(void)
{
    static int env = -2;   /* TNFS_SHINE_SOFT=0/1 overrides the setting, for a check */
    if (env == -2) env = getenv("TNFS_SHINE_SOFT") != NULL ? atoi(getenv("TNFS_SHINE_SOFT")) != 0 : -1;
    if (render_scale == 1) return 0;   /* at render scale 1 a texel is a few pixels at most and the shine's edge a straight cut, as before, whatever the setting or the env say */
    return env >= 0 ? env : soft_setting;
}

/* 0 not shined, 1 the paint's table, 2 the glass's: for the pixel at (x, y) whose texture place is (u, v) in texels */
static int soft_pick_in(const Shine *sh, float u, float v, int x, int y)
{
    const float fu = u - 0.5f, fv = v - 0.5f;
    const float bu = floorf(fu), bv = floorf(fv), tu = fu - bu, tv = fv - bv;
    float paint = 0.0f, glass = 0.0f;
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++) {
            const float w = (i ? tu : 1.0f - tu) * (j ? tv : 1.0f - tv);
            const uint8_t k = texel_class(sh, bu + (float)i, bv + (float)j);
            if (k == SHINE_PAINT || k == SHINE_BODY) paint += w;
            else if (k == SHINE_GLASS) glass += w;
        }
    const float t = BAYER[y & 3][x & 3];
    if (t >= paint + glass) return 0;
    return t < paint ? 1 : 2;
}

int shine_draw_soft_pick(const ShineClass *cls, float u, float v, int x, int y)
{
    const Shine sh = { cls, NULL };
    return soft_pick_in(&sh, u, v, x, y);
}

/* the class the pixel (x, y) at texture place (u, v) is shined by: the texel's own, or with soft edges the table the weights pick (255: not shined). Both passes, the camera's and the exe's, take it. */
static uint8_t pixel_class(const Shine *sh, float u, float v, int x, int y)
{
    if (!soft_on()) return texel_class(sh, u, v);
    const int pick = soft_pick_in(sh, u, v, x, y);
    return pick == 0 ? 255 : pick == 1 ? SHINE_PAINT : SHINE_GLASS;
}

static void triangle(const QuadTarget *q, const Screen *a, const Screen *b, const Screen *c, Outline o, float origin, float grow, const Shine *sh)
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
    const float min_bc = reach(b, c, o.outer_bc, grow), min_ca = reach(c, a, o.outer_ca, grow), min_ab = reach(a, b, o.outer_ab, grow);
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
            const uint8_t k = pixel_class(sh, u, v, x, y);
            /* the mask drives the shine: paint (0) at the body intensity, tinted by the paint; glass and lamps (128) at the glass
             * intensity, keeping their own colour; a body texel (192) keeps its colour too but takes the paint's intensity and table;
             * matte (255) and the rest not at all */
            if (k != SHINE_PAINT && k != SHINE_GLASS && k != SHINE_BODY) continue;
            const float eu = (wa * a->eu + wb * b->eu + wc * c->eu) * r, ev = (wa * a->ev + wb * b->ev + wc * c->ev) * r;
            envmap_pixel(&row[x], sh->env, k == SHINE_GLASS, eu, ev, x, y);
        }
    }
}

static void fan(const QuadTarget *q, const Screen *s, int n, float origin, float grow, const Shine *sh)
{
    for (int i = 1; i + 1 < n; i++) triangle(q, &s[0], &s[i], &s[i + 1], (Outline){ i == 1, 1, i + 2 == n }, origin, grow, sh);
}

enum { COVER_REACH = 16384,   /* the screen coordinates a cover is taken for: the mapper keeps them in 16.16 */
       NOT_DRAWN = 0xff };    /* a cover's pixel the mapper left */

/* the polygon as the exe's mapper was asked to draw it */
typedef struct {
    ObjShape shape;
    const FshEntry *texture;
    MapMode mode;
} Drawn;

/* The pixels the mapper draws for `d` at `screen` within `whole`'s clip: a box of them (*x0, *y0, *bw x *bh bytes, NOT_DRAWN where it left
 * them), malloc'd; NULL when nothing is in the clip, the corners are past COVER_REACH or out of memory. The mapper draws the polygon itself,
 * shifted to the box (it takes whole pixels: a shift does not move its edges or its clip). */
static uint8_t *mapper_cover(const QuadTarget *whole, const Drawn *d, const int32_t screen[8], const int32_t uv[8], int corners, int *x0, int *y0,
                             int *bw, int *bh)
{
    uint8_t one[256];   /* the remap's and the shade's table: every pixel drawn as 1 (on the stack: the bands replay this on several threads) */
    memset(one, 1, sizeof one);
    int32_t lo_x = screen[0], hi_x = screen[0], lo_y = screen[1], hi_y = screen[1];
    for (int i = 1; i < corners; i++) {
        lo_x = screen[2 * i] < lo_x ? screen[2 * i] : lo_x, hi_x = screen[2 * i] > hi_x ? screen[2 * i] : hi_x;
        lo_y = screen[2 * i + 1] < lo_y ? screen[2 * i + 1] : lo_y, hi_y = screen[2 * i + 1] > hi_y ? screen[2 * i + 1] : hi_y;
    }
    const int32_t left = lo_x - 2 > whole->clip_left ? lo_x - 2 : whole->clip_left, right = hi_x + 3 < whole->clip_right ? hi_x + 3 : whole->clip_right;
    const int32_t top = lo_y - 2 > whole->clip_top ? lo_y - 2 : whole->clip_top, bottom = hi_y + 3 < whole->clip_bottom ? hi_y + 3 : whole->clip_bottom;
    if (left >= right || top >= bottom || d->texture == NULL) return NULL;
    if (lo_x < -COVER_REACH || hi_x > COVER_REACH || lo_y < -COVER_REACH || hi_y > COVER_REACH) return NULL;   /* the mapper's 16.16 would wrap: nothing to match */
    *x0 = left, *y0 = top, *bw = right - left, *bh = bottom - top;
    uint8_t *mask = malloc((size_t)*bw * (size_t)*bh);
    if (mask == NULL) return NULL;
    memset(mask, NOT_DRAWN, (size_t)*bw * (size_t)*bh);
    const QuadTarget box = { mask, *bw, 0, 0, *bw, *bh };
    int32_t at[8];
    for (int i = 0; i < corners; i++) at[2 * i] = screen[2 * i] - left, at[2 * i + 1] = screen[2 * i + 1] - top;
    for (int i = corners; i < 4; i++) at[2 * i] = at[0], at[2 * i + 1] = at[1];
    /* the polygon's own mode, which never writes 0xff where it skips the texel: a remap mode's and the shade's table is all 1 here; the opaque
     * mode, which writes 0xff texels, through the remap (the same mapper with each texel looked up) */
    const MapMode m = d->mode == MAP_OPAQUE ? MAP_REMAP : d->mode;
    switch (d->shape) {
    case OBJ_TRIANGLE: obj_draw_triangle(&box, d->texture, uv, at, m, one); break;
    case OBJ_QUAD: obj_draw_quad(&box, d->texture, uv, at, m, one); break;
    case OBJ_QUAD_TRIS: obj_draw_quad_tris(&box, d->texture, uv, at, m, one); break;
    case OBJ_FULL: obj_draw_full(&box, d->texture, at, m, one); break;
    case OBJ_INSET: obj_draw_inset(&box, d->texture, at, m, one); break;
    }
    return mask;
}

/* the weights of (px, py) in the triangle a b c (any winding; outside it they extrapolate, as a pixel the mapper's rounding puts past an edge) */
static int weights(const Screen *a, const Screen *b, const Screen *c, float px, float py, float w[3])
{
    const float area = edge(a, b, c->sx, c->sy);
    if (fabsf(area) < 1e-6f) return 0;
    w[0] = edge(b, c, px, py) / area, w[1] = edge(c, a, px, py) / area, w[2] = edge(a, b, px, py) / area;
    return 1;
}

/* `eo` is the envmap's uv per corner, 2 floats each. Every pixel the mapper draws (mapper_cover) within `q`'s clip (a band's rows when replayed),
 * once; its uvs from the fan's triangle on its side of the diagonal (a quad's 0-2: corner 1's side or corner 3's) */
static void draw_affine(const QuadTarget *q, const QuadTarget *whole, const Shine *sh, const Drawn *d, const int32_t screen[8], const int32_t uv[8],
                        const float eo[8], int corners)
{
    Screen s[4];
    for (int i = 0; i < corners; i++)
        s[i] = (Screen){ (float)screen[2 * i], (float)screen[2 * i + 1], 1.0f, (float)uv[2 * i], (float)uv[2 * i + 1], eo[2 * i], eo[2 * i + 1] };
    int x0 = 0, y0 = 0, bw = 0, bh = 0;
    uint8_t *mask = mapper_cover(whole, d, screen, uv, corners, &x0, &y0, &bw, &bh);
    if (mask == NULL) return;
    /* a quad's side of corner 1 across the 0-2 diagonal; corner 1 on it (a quad drawn as a triangle: a corner given twice) leaves corner 3's */
    const float side1 = corners == 4 ? edge(&s[0], &s[2], s[1].sx, s[1].sy) : 0.0f, side3 = corners == 4 ? edge(&s[0], &s[2], s[3].sx, s[3].sy) : 0.0f;
    for (int by = 0; by < bh; by++) {
        const int y = y0 + by;
        if (y < q->clip_top || y >= q->clip_bottom) continue;
        uint8_t *row = q->pixels + (size_t)y * (size_t)q->pitch;
        for (int bx = 0; bx < bw; bx++) {
            const int x = x0 + bx;
            if (mask[(size_t)by * (size_t)bw + (size_t)bx] == NOT_DRAWN || x < q->clip_left || x >= q->clip_right) continue;
            const Screen *b = &s[1], *c = &s[2];
            if (corners == 4 && (side1 == 0.0f || edge(&s[0], &s[2], (float)x, (float)y) * side1 < 0)) b = &s[2], c = &s[3];   /* corner 3's triangle */
            float w[3];
            if (!weights(&s[0], b, c, (float)x, (float)y, w) && !(corners == 4 && side3 == 0.0f && weights(&s[0], &s[1], &s[2], (float)x, (float)y, w)))
                continue;   /* both of a quad's triangles flat: a line, nothing to weigh */
            const uint8_t k = pixel_class(sh, w[0] * s[0].uz + w[1] * b->uz + w[2] * c->uz, w[0] * s[0].vz + w[1] * b->vz + w[2] * c->vz, x, y);
            if (k != SHINE_PAINT && k != SHINE_GLASS && k != SHINE_BODY) continue;   /* as triangle(): paint, glass and body texels shine */
            envmap_pixel(&row[x], sh->env, k == SHINE_GLASS, w[0] * s[0].eu + w[1] * b->eu + w[2] * c->eu, w[0] * s[0].ev + w[1] * b->ev + w[2] * c->ev, x, y);
        }
    }
    free(mask);
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
    fan(q, s, n, 0.5f, EDGE_GROW, sh);
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
        const Drawn d = { (ObjShape)c->a, c->texture, (MapMode)c->b };
        unpack_corners(c->v + 16, c->count, eo);
        draw_affine(&band, whole, &sh, &d, c->v, c->v + 8, eo, c->count);
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
                           const int32_t packed[4], int corners, ObjShape shape, const FshEntry *texture, MapMode mode)
{
    if (!usable(cls, corners) || env == NULL || env->image == NULL || texture == NULL) return;
    struct RasterRecorder *rec = raster_recorder_for(q);
    if (rec != NULL) {
        RasterCmd *c = raster_record_add(rec, q, replay);
        c->mode = KIND_AFFINE, c->count = corners, c->table = cls, c->shape = (const uint8_t *)env;
        c->texture = texture, c->a = (int32_t)shape, c->b = (int32_t)mode;
        memcpy(c->v, screen, 8 * sizeof *screen);
        memcpy(c->v + 8, uv, 8 * sizeof *uv);
        memcpy(c->v + 16, packed, 4 * sizeof *packed);
        raster_cmd_rows(c, screen, corners);
        return;
    }
    float eo[8];
    unpack_corners(packed, corners, eo);
    const Shine sh = { cls, env };
    const Drawn d = { shape, texture, mode };
    draw_affine(q, q, &sh, &d, screen, uv, eo, corners);
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
