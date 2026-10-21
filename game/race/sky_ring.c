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
 * flips with the mirrored tracks. */
static int slice_visible(const int32_t q[8], int mirrored, int32_t clip_right, int32_t clip_left)
{
    const int32_t x0 = q[0], x1 = q[2], x2 = q[4], x3 = q[6];
    if (!mirrored) {
        if (!((x0 <= clip_right || x1 <= clip_right) && (clip_left <= x2 || clip_left <= x3) && x0 <= x1 && x3 <= x2))
            return 0;
    } else {
        if (!((x2 <= clip_right || x3 <= clip_right) && (clip_left <= x0 || clip_left <= x1) && x1 <= x0 && x2 <= x3))
            return 0;
    }
    return x0 > -CULL_LIMIT && x1 > -CULL_LIMIT && x2 > -CULL_LIMIT && x3 > -CULL_LIMIT && x0 < CULL_LIMIT &&
           x1 < CULL_LIMIT && x2 < CULL_LIMIT && x3 < CULL_LIMIT;
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

/* port_wide: a slice's end that projects behind the eye or past the cull limit */
static int port_off(int ok, const int32_t s[2])
{
    return !ok || s[0] <= -CULL_LIMIT || s[0] >= CULL_LIMIT;
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

/* port_wide: a slice with one end off cut where it leaves the view (the nearer edge it crosses), that end's u moved
 * along the cell with it; 0 when nothing of it is left */
static int port_clip(const SkyFrame *f, const int32_t cam[2][3], int32_t screen[2][2], int off, int32_t uv[8])
{
    const int on = 1 - off;
    const double tl = port_edge_t(f->view, cam[on], cam[off], f->clip_left - PORT_EDGE_PAD);
    const double tr = port_edge_t(f->view, cam[on], cam[off], f->clip_right + PORT_EDGE_PAD);
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
    if (!f->sky_off) sky_fills(&f->viewport, roll, f->horizon, below, above, top, bottom, sink);
    for (int k = 0; k < slices; k++) {
        const int32_t cell = start + k;
        const int set = info->sky_flag != 0 && ((cell >> FINAL_FLAG_BIT) & 1);
        int32_t uv[8], ends[2][2] = { { screen[k][0], screen[k][1] }, { screen[k + 1][0], screen[k + 1][1] } };
        for (int i = 0; i < 8; i++) uv[i] = ring->uv[set][cell & 7][i];
        if (extra != 0) {
            const int off0 = port_off(ok[k], screen[k]), off1 = port_off(ok[k + 1], screen[k + 1]);
            if (off0 && off1) continue;
            const int32_t ends_cam[2][3] = { { cam[k][0], cam[k][1], cam[k][2] },
                                             { cam[k + 1][0], cam[k + 1][1], cam[k + 1][2] } };
            if ((off0 || off1) && !port_clip(f, ends_cam, ends, off0 ? 0 : 1, uv)) continue;
        }
        const int32_t *a = ends[0], *b = ends[1];
        const int32_t q[8] = { add(a[0], dx_up), add(a[1], dy_up), add(b[0], dx_up), add(b[1], dy_up),
                               sub(b[0], dx_down), sub(b[1], dy_down), sub(a[0], dx_down), sub(a[1], dy_down) };
        if (!slice_visible(q, f->mirrored, f->clip_right, f->clip_left)) continue;
        if (f->sky_off) {
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
