#include "game/race/sky_ring.h"

#include "game/sys/fixed_math.h"

#include <math.h>

enum {
    RING_STEP = 0x1000,        /* 1/16 turn in 16-bit angles */
    RING_WINDOW_BIAS = 0x320000,
    RING_WINDOW_UNIT = 0x100000,   /* 1/16 turn in 24-bit angles */
    SLICE_V_MARGIN = 2,
    FILL_PAD = 4,              /* 0043b0c8: left fills are 4 pixels wider, edges 4 higher */
    FILL_SLOPE_PAD = 3,
    CULL_LIMIT = 0x500,
    FINAL_FLAG_BIT = 3,        /* cells 8..15 use the mirrored set (004fcd74) */
    PORT_EXTRA = 2,            /* port_wide: slices added on each side */
    PORT_EDGE_PAD = 16,        /* port_wide: a clipped slice ends this far past the view's edge */
    PORT_PROBE_Z = 0x1000      /* port_wide: the depth the focal length is measured at */
};

void sky_ring_build(SkyRing *ring, const TrackInfo *info, const FshEntry *horz)
{
    *ring = (SkyRing){ 0 };
    ring->enabled = info->height_far > 0;
    if (info->ring == 0) return;
    /* 0043abfc: cells are width / 8 wide, less one texel; v spans the
     * bitmap's rows except the first and the last. */
    const int32_t width = horz->width, height = horz->height;
    const int32_t cell = (width >> 3) - 1;
    const int32_t v_bottom = (int32_t)((uint32_t)(height - SLICE_V_MARGIN) << 16);
    for (int32_t k = 0; k < SKY_RING_CELLS; k++) {
        int32_t *a = ring->uv[0][k], *b = ring->uv[1][k];
        a[0] = ((k * width) >> 3) << 16;
        a[2] = a[0] + cell * 0x10000;
        a[4] = a[2];
        a[6] = a[0];
        a[1] = a[3] = 0x10000;
        a[5] = a[7] = v_bottom;
        b[0] = (((SKY_RING_CELLS - 1 - k) * width) >> 3) * 0x10000 + cell * 0x10000;
        b[2] = b[0] - cell * 0x10000;
        b[4] = b[2];
        b[6] = b[0];
        b[1] = b[3] = 0x10000;
        b[5] = b[7] = v_bottom;
    }
    ring->slices = SKY_RING_SLICES;
    ring->radius = info->radius_far;
    ring->vert_offset = info->vert_offset_far;
    ring->z_offset = info->z_offset_far;
    for (int32_t i = 0; i < SKY_RING_POINTS; i++) {
        const int32_t angle = i * RING_STEP;
        ring->points[i][0] = fx_mul_round(fx_sin16(angle), ring->radius);
        ring->points[i][1] = ring->vert_offset;
        ring->points[i][2] = (int32_t)((uint32_t)fx_mul_round(fx_cos16(angle), ring->radius) + (uint32_t)ring->z_offset);
    }
}

/* The original wraps; INT32_MIN marks a point behind the eye and is still
 * offset. */
static int32_t add(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a + (uint32_t)b);
}

static int32_t sub(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a - (uint32_t)b);
}

/* imul r32, r32; sar 16: the product truncated to 32 bits first. */
static int32_t mul32_shift(int32_t a, int32_t b)
{
    return (int32_t)((uint32_t)a * (uint32_t)b) >> 16;
}

/* 0043b298: the first of the 7 + 1 points, from the camera yaw. */
static int32_t window_start(int32_t yaw)
{
    const int32_t turn = sub(yaw, RING_WINDOW_BIAS);
    /* sar after a bias that makes it truncate towards zero, then % 16 */
    const int32_t cells = add(turn, turn < 0 ? RING_WINDOW_UNIT - 1 : 0) >> 20;
    const int32_t start = cells % 16;
    return start < 0 ? start + 16 : start;
}

/* 0043b0c8: the sky above and below the ring. below / above are the ring
 * extents (height - offset and height + offset, scaled) in EBX and ECX; the
 * horizon is a point of the ring's far edge, the edges slope with the roll. */
static void sky_fills(const SkyViewport *vp, int32_t roll, const int32_t horizon[2], int32_t below, int32_t above,
                      uint8_t top, uint8_t bottom, const SkySink *sink)
{
    const int32_t angle = roll >> 8;
    const int32_t cosine = fx_cos16(angle);
    const int32_t lo = sub(fx_div(below - 1, cosine), FILL_PAD);
    const int32_t hi = sub(fx_div(above - 1, cosine), FILL_PAD);
    const int32_t right = sub(add(vp->x, vp->w), horizon[0]);
    const int32_t tangent = fx_div(fx_sin16(angle), cosine);
    const int32_t drop = fx_mul_round(fx_mul_round(right, tangent), vp->scale);
    const int32_t far_edge = add(horizon[1], hi);
    int32_t top_left, top_right, bottom_left, bottom_right;
    if (drop > 0) {
        top_left = sub(horizon[1], lo);
        top_right = add(top_left, drop);
        bottom_right = far_edge;
        bottom_left = sub(far_edge, drop);
    } else {
        top_left = sub(sub(horizon[1], lo), drop);
        top_right = sub(horizon[1], lo);
        bottom_left = far_edge;
        bottom_right = add(far_edge, drop);
    }
    const int32_t end = add(vp->y, vp->h);
    sink->fill(sink->user, vp->x, vp->y, add(right, FILL_PAD), sub(top_left, vp->y), top);
    sink->fill(sink->user, vp->x, bottom_left, add(right, FILL_PAD), sub(end, bottom_left), bottom);
    sink->fill(sink->user, horizon[0], vp->y, right, add(sub(top_right, vp->y), FILL_SLOPE_PAD), top);
    sink->fill(sink->user, horizon[0], bottom_right, right, sub(end, bottom_right), bottom);
}

/* 0043b298's culling of one slice; q = 4 corners (x, y). The corner order
 * flips with the mirrored tracks. port: `any_corner` (port_wide) lets any corner count for the view's edges, not only
 * the exe's top two against the right and bottom two against the left: with the camera rolled those corners move
 * sideways apart, and a slice with only its other corners in view was dropped, a black wedge at the side. The same
 * test with the roll at 0. */
static int slice_visible(const int32_t q[8], int mirrored, int32_t clip_right, int32_t clip_left, int32_t limit,
                         int any_corner)
{
    const int32_t x0 = q[0], x1 = q[2], x2 = q[4], x3 = q[6];
    if (any_corner) {
        const int32_t lo01 = x0 < x1 ? x0 : x1, lo23 = x2 < x3 ? x2 : x3;
        const int32_t hi01 = x0 < x1 ? x1 : x0, hi23 = x2 < x3 ? x3 : x2;
        const int32_t lo = lo01 < lo23 ? lo01 : lo23, hi = hi01 < hi23 ? hi23 : hi01;
        if (!(lo <= clip_right && clip_left <= hi && (mirrored ? x1 <= x0 && x2 <= x3 : x0 <= x1 && x3 <= x2)))
            return 0;
    } else if (!mirrored) {
        if (!((x0 <= clip_right || x1 <= clip_right) && (clip_left <= x2 || clip_left <= x3) && x0 <= x1 && x3 <= x2))
            return 0;
    } else {
        if (!((x2 <= clip_right || x3 <= clip_right) && (clip_left <= x0 || clip_left <= x1) && x1 <= x0 && x2 <= x3))
            return 0;
    }
    return x0 > -limit && x1 > -limit && x2 > -limit && x3 > -limit && x0 < limit && x1 < limit && x2 < limit &&
           x3 < limit;
}

/* 004252ec: a quad as two triangles (0, 1, 2) and (2, 3, 0). */
static void draw_quad(const SkySink *sink, const FshEntry *texture, const int32_t uv16[8], const int32_t q[8],
                      int shift)
{
    int32_t s[10], uv[6];
    for (int i = 0; i < 8; i++) s[i] = q[i] >> shift;
    s[8] = s[0], s[9] = s[1];
    for (int i = 0; i < 6; i++) uv[i] = uv16[i] >> 16;
    sink->triangle(sink->user, texture, uv, s);
    const int32_t uv2[6] = { uv16[4] >> 16, uv16[5] >> 16, uv16[6] >> 16, uv16[7] >> 16, uv16[0] >> 16,
                             uv16[1] >> 16 };
    sink->triangle(sink->user, texture, uv2, s + 4);
}

/* port_wide: where the camera's roll puts a slice's corners sideways of its ends on the ring (lo, hi: the leftmost and
 * rightmost of dx_up and -dx_down), the pads a clipped end is taken past the view's left and right edges by (enough
 * that both its corners still reach the edge), and the cull limit (the exe's, or past the padded edges when a wide or
 * rolled view needs more) */
typedef struct {
    int32_t lo, hi, pad_left, pad_right, limit;
} PortReach;

static PortReach port_reach(const SkyFrame *f, int32_t dx_up, int32_t dx_down)
{
    const int32_t down = sub(0, dx_down);
    const int32_t lo = dx_up < down ? dx_up : down, hi = dx_up < down ? down : dx_up;
    const int32_t pad = PORT_EDGE_PAD * f->pixel_scale;
    const int32_t pad_left = hi > pad ? hi : pad;
    const int32_t pad_right = -lo > pad ? -lo : pad;
    const int32_t right = f->clip_right + pad_right + hi + 1, left = pad_left - f->clip_left - lo + 1;
    const int32_t needed = right > left ? right : left;
    const int32_t limit = CULL_LIMIT * f->pixel_scale;
    return (PortReach){ lo, hi, pad_left, pad_right, needed > limit ? needed : limit };
}

/* port_wide: a slice's end that projects behind the eye or so far out that a corner of the slice there would pass
 * the cull limit (and cull the whole slice) */
static int port_off(int ok, const int32_t s[2], const PortReach *r)
{
    return !ok || s[0] + r->lo <= -r->limit || s[0] + r->hi >= r->limit;
}

/* port_wide: the point of camera-space segment a -> b (a in front) whose x projects to `edge`, as the fraction t of
 * the way from a; 0 when the segment does not get there */
static double port_edge_t(const View *view, const int32_t a[3], const int32_t b[3], int32_t edge)
{
    const int32_t centre[3] = { 0, 0, PORT_PROBE_Z }, side[3] = { PORT_PROBE_Z, 0, PORT_PROBE_Z };
    int32_t c[2], s[2];
    view_project(view, centre, c);
    view_project(view, side, s);
    const double focal = s[0] - c[0], d = edge - c[0];
    const double den = focal * ((double)b[0] - a[0]) - d * ((double)b[2] - a[2]);
    if (den == 0.0) return 0.0;
    const double t = (d * a[2] - focal * a[0]) / den;
    return t > 0.0 && t <= 1.0 ? t : 0.0;
}

/* port_wide: a slice with one end off cut where it leaves the view (the nearer padded edge it crosses), that end's u
 * moved along the cell with it; 0 when nothing of it is left */
static int port_clip(const SkyFrame *f, const int32_t cam[2][3], int32_t screen[2][2], int off, int32_t uv[8],
                     const PortReach *r)
{
    const int on = 1 - off;
    const double tl = port_edge_t(f->view, cam[on], cam[off], f->clip_left - r->pad_left);
    const double tr = port_edge_t(f->view, cam[on], cam[off], f->clip_right + r->pad_right);
    const double t = tl == 0.0 ? tr : tr == 0.0 ? tl : tl < tr ? tl : tr;
    if (t == 0.0) return 0;
    int32_t p[3];
    for (int c = 0; c < 3; c++) p[c] = (int32_t)lround(cam[on][c] + t * ((double)cam[off][c] - cam[on][c]));
    if (!view_project(f->view, p, screen[off])) return 0;
    /* corners 0 and 3 sit at the slice's first end, 1 and 2 at its second */
    const int32_t u_on = on == 0 ? uv[0] : uv[2], u_off = on == 0 ? uv[2] : uv[0];
    const int32_t u = (int32_t)lround(u_on + t * ((double)u_off - u_on));
    if (off == 0) uv[0] = uv[6] = u;
    else uv[2] = uv[4] = u;
    return 1;
}

typedef struct {
    const SkyRing *ring;
    const TrackInfo *info;
    const SkyFrame *f;
    const PortReach *reach;
    int32_t start;
    int extra;
    int32_t d[4];   /* dx_up, dy_up, dx_down, dy_down */
    const int32_t (*screen)[2];
    const int32_t (*cam)[3];
    const int *ok;
} Slices;

/* slice k's uv (16.16), its ends on the ring (clipped with port_wide) and its 4 corners; 0 when it is not drawn */
static int slice_quad(const Slices *s, int k, int32_t uv[8], int32_t ends[2][2], int32_t q[8])
{
    const SkyFrame *f = s->f;
    const int32_t cell = s->start + k;
    const int set = s->info->sky_flag != 0 && ((cell >> FINAL_FLAG_BIT) & 1);
    for (int i = 0; i < 2; i++) ends[i][0] = s->screen[k + i][0], ends[i][1] = s->screen[k + i][1];
    for (int i = 0; i < 8; i++) uv[i] = s->ring->uv[set][cell & 7][i];
    if (s->extra != 0) {
        const int off0 = port_off(s->ok[k], s->screen[k], s->reach);
        const int off1 = port_off(s->ok[k + 1], s->screen[k + 1], s->reach);
        if (off0 && off1) return 0;
        const int32_t ends_cam[2][3] = { { s->cam[k][0], s->cam[k][1], s->cam[k][2] },
                                         { s->cam[k + 1][0], s->cam[k + 1][1], s->cam[k + 1][2] } };
        if ((off0 || off1) && !port_clip(f, ends_cam, ends, off0 ? 0 : 1, uv, s->reach)) return 0;
    }
    const int32_t *a = ends[0], *b = ends[1];
    const int32_t corners[8] = { add(a[0], s->d[0]), add(a[1], s->d[1]), add(b[0], s->d[0]), add(b[1], s->d[1]),
                                 sub(b[0], s->d[2]), sub(b[1], s->d[3]), sub(a[0], s->d[2]), sub(a[1], s->d[3]) };
    for (int i = 0; i < 8; i++) q[i] = corners[i];
    return slice_visible(q, f->mirrored, f->clip_right, f->clip_left, s->reach->limit, s->extra != 0);
}

/* port_wide: under the fills and the slices, each drawn slice's sky above its top edge and below its bottom edge in
 * the fills' colours. The exe's fills step the band's line in two rectangles that slope with the roll alone; with the
 * camera pitched far (a jump, a roll-over) and rolled the band's ends leave them, and the view (cleared, where the
 * exe kept the last frame) showed black wedges there. Pixels the exe's fills and slices draw are theirs, as before. */
static void port_backing(const Slices *s, int slices, uint8_t top, uint8_t bottom, const SkySink *sink)
{
    const SkyViewport *vp = &s->f->viewport;
    const int32_t floor = add(vp->y, vp->h);
    for (int k = 0; k < slices; k++) {
        int32_t uv[8], ends[2][2], q[8];
        if (!slice_quad(s, k, uv, ends, q)) continue;
        /* the edges held inside the viewport's rows, so each polygon stays convex (flat_polygon's input) */
        const int32_t up[8] = { q[0], vp->y, q[2], vp->y, q[2], q[3] < vp->y ? vp->y : q[3],
                                q[0], q[1] < vp->y ? vp->y : q[1] };
        const int32_t down[8] = { q[6], floor, q[4], floor, q[4], q[5] > floor ? floor : q[5],
                                  q[6], q[7] > floor ? floor : q[7] };
        sink->polygon(sink->user, top, up);
        sink->polygon(sink->user, bottom, down);
    }
}

void sky_ring_draw(const SkyRing *ring, const TrackInfo *info, const SkyFrame *f, const FshEntry *horz,
                   const SkySink *sink)
{
    if (f->reference_width == 0) return;
    const int extra = f->port_wide != 0 && ring->slices > 0 ? PORT_EXTRA : 0;
    const int32_t start = (window_start(f->yaw) - extra + 16) % 16;
    const int slices = (ring->slices < SKY_RING_SLICES ? ring->slices : SKY_RING_SLICES) + 2 * extra;
    /* 0048cf08, 0048cf60, 0048cdd0: ring points around the camera, in camera
     * space, projected. */
    int32_t screen[SKY_RING_SLICES + 2 * PORT_EXTRA + 1][2], cam[SKY_RING_SLICES + 2 * PORT_EXTRA + 1][3];
    int ok[SKY_RING_SLICES + 2 * PORT_EXTRA + 1];
    for (int i = 0; i <= slices; i++) {
        int32_t p[3];
        for (int c = 0; c < 3; c++)
            p[c] = add(ring->points[start + i][c] >> (f->world_shift & 31), f->position[c] >> (f->world_shift & 31));
        mat3_apply(p, f->matrix, cam[i]);
        ok[i] = view_project(f->view, cam[i], screen[i]);
    }
    /* port (render scale): the ring's heights are in pixels of the frame, so they grow with it */
    const int32_t scale = (int32_t)((uint32_t)f->viewport.scale * (uint32_t)f->viewport.width) / f->reference_width;
    const int32_t offset = fx_mul_round(info->ring_y_offset, scale);
    const int32_t height = fx_mul_round(info->ring_height, scale);
    const int32_t roll = f->mirrored ? sub(0, f->roll) : f->roll;
    const int32_t angle = roll >> 8;
    const int32_t sin_a = fx_sin16(angle), cos_a = fx_cos16(angle);
    const int32_t below = sub(height, offset), above = add(height, offset);
    const int32_t dx_up = mul32_shift(sin_a, below), dy_up = sub(0, mul32_shift(cos_a, below));
    const int32_t dx_down = mul32_shift(sin_a, above), dy_down = sub(0, mul32_shift(cos_a, above));
    /* The first and last pixels of the bitmap colour the sky. */
    const uint8_t top = horz->payload[0];
    const uint8_t bottom = horz->payload[(size_t)horz->width * horz->height - 1];
    /* the exe's own slices: its cull limit, no pads */
    const PortReach reach = extra != 0 ? port_reach(f, dx_up, dx_down)
                                       : (PortReach){ 0, 0, PORT_EDGE_PAD, PORT_EDGE_PAD, CULL_LIMIT * f->pixel_scale };
    const Slices all = { ring, info, f, &reach, start, extra, { dx_up, dy_up, dx_down, dy_down }, screen, cam, ok };
    if (extra != 0 && !f->sky_off && sink->polygon != NULL) port_backing(&all, slices, top, bottom, sink);
    if (!f->sky_off) sky_fills(&f->viewport, roll, f->horizon, below, above, top, bottom, sink);
    for (int k = 0; k < slices; k++) {
        int32_t uv[8], ends[2][2], q[8];
        if (!slice_quad(&all, k, uv, ends, q)) continue;
        if (f->sky_off) {
            const int32_t *a = ends[0], *b = ends[1];
            const int32_t floor = add(f->viewport.y, f->viewport.h);
            const int32_t up[8] = { a[0], f->viewport.y, b[0], f->viewport.y, b[0], b[1], a[0], a[1] };
            const int32_t down[8] = { a[0], floor, b[0], floor, b[0], b[1], a[0], a[1] };
            sink->polygon(sink->user, top, up);
            sink->polygon(sink->user, bottom, down);
            continue;
        }
        draw_quad(sink, horz, uv, q, f->screen_shift);
    }
}
