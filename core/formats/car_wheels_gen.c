/* A car's wheels made by the build (car_wheels_gen.h): the four tyre frames from one picture. Integer arithmetic only (Q16 pixels, Q30 radians and
 * sines), so a frame's bytes do not depend on the machine's libm or on how its compiler fuses floating point. */
#include "formats/car_wheels_gen.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    OPAQUE = 128,               /* the alpha from which a pixel is the wheel's, for its circle */
    SAMPLE_STEP_Q30 = 28106436  /* the most an arc's samples are apart: 1.5 degrees in Q30 radians */
};
#define ONE_Q16 65536LL
#define ONE_Q30 (1LL << 30)
#define PI_Q30 3373259426LL   /* pi in Q30 */
#define HALF_PI_Q30 1686629713LL

void wheel_frame_options_default(WheelFrameOptions *o)
{
    memset(o, 0, sizeof *o);
    o->tyre_share = 0.32;
    o->slow_angle_deg = 20.0;
    o->fast_arc_deg = 72.0;
}

/* x rounded to the nearest integer, halves away from 0 (no libm: lround's own rounding is the same everywhere, but this keeps the file free of it) */
static int64_t round_to_int(double x)
{
    return x < 0 ? -(int64_t)(-x + 0.5) : (int64_t)(x + 0.5);
}

static int64_t deg_to_q30(double deg)
{
    return round_to_int(deg * 18740330.2771369);   /* pi / 180 * 2^30 */
}

/* sin of `a` (Q30 radians, any value) in Q30: reduced to -pi/2..pi/2, then its Taylor series to x^15 (the error is under 2^-30 there) */
static int64_t sin_q30(int64_t a)
{
    const int64_t turn = 2 * PI_Q30;
    a %= turn;
    if (a > PI_Q30) a -= turn;
    if (a < -PI_Q30) a += turn;
    if (a > HALF_PI_Q30) a = PI_Q30 - a;
    if (a < -HALF_PI_Q30) a = -PI_Q30 - a;
    const int64_t x2 = (a * a) >> 30;
    int64_t term = a, sum = a;
    for (int k = 1; k <= 7; k++) {
        term = -((term * x2) >> 30) / ((2 * k) * (2 * k + 1));
        sum += term;
    }
    return sum;
}

static int64_t cos_q30(int64_t a)
{
    return sin_q30(a + HALF_PI_Q30);
}

/* the integer square root of v >= 0 */
static int64_t isqrt64(int64_t v)
{
    uint64_t x = (uint64_t)v, r = 0, bit = 1ull << 62;
    while (bit > x) bit >>= 2;
    while (bit != 0) {
        if (x >= r + bit) x -= r + bit, r = (r >> 1) + bit;
        else r >>= 1;
        bit >>= 2;
    }
    return (int64_t)r;
}

int wheel_frame_find_circle(const uint8_t *rgba, int w, int h, double *cx, double *cy, double *radius, ExchangeError *err)
{
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (rgba[4 * ((size_t)y * (size_t)w + (size_t)x) + 3] >= OPAQUE) {
                x0 = x < x0 ? x : x0, x1 = x > x1 ? x : x1;
                y0 = y < y0 ? y : y0, y1 = y > y1 ? y : y1;
            }
    if (x1 < 0) {
        exchange_fail(err, "the wheel picture has no opaque pixel (alpha 128 or more): it needs the wheel on a transparent background");
        return 0;
    }
    *cx = (x0 + x1 + 1) / 2.0, *cy = (y0 + y1 + 1) / 2.0;
    const int side = x1 - x0 > y1 - y0 ? x1 - x0 + 1 : y1 - y0 + 1;
    *radius = side / 2.0;
    return 1;
}

typedef struct {
    const uint8_t *rgba;
    int w, h;
    int64_t cx, cy, inner;   /* Q16: the centre, and the radius inside which the frame is turned and blurred (the tyre band is outside it) */
} Wheel;

/* adds the picture at (sx, sy) (Q16, a pixel's centre at +0.5) bilinearly, each pixel weighted by its alpha, to sum[0..2] (colour x weight) and *weight */
static void add_sample(const Wheel *wh, int64_t sx, int64_t sy, int64_t sum[3], int64_t *weight)
{
    sx -= ONE_Q16 / 2, sy -= ONE_Q16 / 2;
    const int64_t ix = sx >> 16, iy = sy >> 16, fx = sx & (ONE_Q16 - 1), fy = sy & (ONE_Q16 - 1);
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++) {
            const int64_t x = ix + i, y = iy + j;
            if (x < 0 || y < 0 || x >= wh->w || y >= wh->h) continue;
            const int64_t bw = (i ? fx : ONE_Q16 - fx) * (j ? fy : ONE_Q16 - fy) >> 16;   /* Q16 */
            const uint8_t *p = wh->rgba + 4 * ((size_t)y * (size_t)wh->w + (size_t)x);
            const int64_t wgt = bw * p[3];
            for (int c = 0; c < 3; c++) sum[c] += wgt * p[c];
            *weight += wgt;
        }
}

/* one frame: the picture turned by `turn` and blurred along `arc` (both Q30 radians) inside the tyre band, the rest and every alpha as they are */
static void make_frame(const Wheel *wh, int64_t turn, int64_t arc, uint8_t *out)
{
    const int samples = arc <= 0 ? 1 : (int)((arc + SAMPLE_STEP_Q30 - 1) / SAMPLE_STEP_Q30) + 1;
    int64_t *cs = malloc(sizeof *cs * 2 * (size_t)samples);
    memcpy(out, wh->rgba, (size_t)wh->w * (size_t)wh->h * 4);
    if (cs == NULL) return;
    for (int s = 0; s < samples; s++) {   /* the samples evenly across the arc, about the turn; out(p) = picture(rotated by -angle (p)) */
        const int64_t angle = samples == 1 ? turn : turn - arc / 2 + arc * s / (samples - 1);
        cs[2 * s] = cos_q30(angle), cs[2 * s + 1] = sin_q30(angle);
    }
    for (int y = 0; y < wh->h; y++)
        for (int x = 0; x < wh->w; x++) {
            const int64_t dx = (int64_t)x * ONE_Q16 + ONE_Q16 / 2 - wh->cx, dy = (int64_t)y * ONE_Q16 + ONE_Q16 / 2 - wh->cy;
            if (isqrt64(dx * dx + dy * dy) >= wh->inner) continue;   /* the tyre band and outside: the picture's */
            int64_t sum[3] = { 0, 0, 0 }, weight = 0;
            for (int s = 0; s < samples; s++) {
                const int64_t c = cs[2 * s], sn = cs[2 * s + 1];
                add_sample(wh, wh->cx + ((dx * c + dy * sn) >> 30), wh->cy + ((dy * c - dx * sn) >> 30), sum, &weight);
            }
            if (weight <= 0) continue;
            uint8_t *p = out + 4 * ((size_t)y * (size_t)wh->w + (size_t)x);
            for (int k = 0; k < 3; k++) p[k] = (uint8_t)((sum[k] + weight / 2) / weight);
        }
    free(cs);
}

static int check_options(const WheelFrameOptions *o, int w, int h, ExchangeError *err)
{
    if (w <= 0 || h <= 0 || w > WHEEL_FRAME_MAX_SIDE || h > WHEEL_FRAME_MAX_SIDE) return exchange_fail(err, "the wheel picture is %d x %d: 1 to %d pixels a side", w, h, WHEEL_FRAME_MAX_SIDE), 0;
    if (!(o->tyre_share > 0.0 && o->tyre_share < 1.0)) return exchange_fail(err, "the tyre share is %g: the tyre is more than 0 and less than 1 of the radius", o->tyre_share), 0;
    if (o->has_radius && !(o->radius > 0.0 && o->radius <= WHEEL_FRAME_MAX_SIDE)) return exchange_fail(err, "the wheel's radius is %g pixels: it must be more than 0", o->radius), 0;
    if (o->has_centre && !(o->centre_x >= 0.0 && o->centre_y >= 0.0 && o->centre_x <= w && o->centre_y <= h))
        return exchange_fail(err, "the wheel's centre %g,%g is not on the %d x %d picture", o->centre_x, o->centre_y, w, h), 0;
    if (!(o->slow_angle_deg >= 0.0 && o->slow_angle_deg <= 180.0)) return exchange_fail(err, "the slow angle is %g degrees: 0 to 180", o->slow_angle_deg), 0;
    if (!(o->fast_arc_deg >= 0.0 && o->fast_arc_deg <= 360.0)) return exchange_fail(err, "the fast arc is %g degrees: 0 to 360", o->fast_arc_deg), 0;
    return 1;
}

int wheel_frames_make(const uint8_t *rgba, int w, int h, const WheelFrameOptions *o, uint8_t *frames[WHEEL_FRAMES], ExchangeError *err)
{
    for (int k = 0; k < WHEEL_FRAMES; k++) frames[k] = NULL;
    if (rgba == NULL || !check_options(o, w, h, err)) return 0;
    double cx = o->centre_x, cy = o->centre_y, radius = o->radius, found_x, found_y, found_r;
    if ((!o->has_centre || !o->has_radius) && !wheel_frame_find_circle(rgba, w, h, &found_x, &found_y, &found_r, err)) return 0;
    if (!o->has_centre) cx = found_x, cy = found_y;
    if (!o->has_radius) radius = found_r;
    const Wheel wh = { rgba, w, h, round_to_int(cx * ONE_Q16), round_to_int(cy * ONE_Q16), round_to_int(radius * (1.0 - o->tyre_share) * ONE_Q16) };
    const int64_t half_slow = deg_to_q30(o->slow_angle_deg) / 2;
    const int64_t turn[WHEEL_FRAMES] = { 0, -half_slow, half_slow, 0 }, arc[WHEEL_FRAMES] = { 0, half_slow, half_slow, deg_to_q30(o->fast_arc_deg) };
    const size_t bytes = (size_t)w * (size_t)h * 4;
    for (int k = 0; k < WHEEL_FRAMES; k++) {
        frames[k] = malloc(bytes);
        if (frames[k] == NULL) {
            for (int j = 0; j < k; j++) free(frames[j]), frames[j] = NULL;
            exchange_fail(err, "out of memory for the wheel frames");
            return 0;
        }
        if (k == WHEEL_FRAME_STILL) memcpy(frames[k], rgba, bytes);
        else make_frame(&wh, turn[k], arc[k], frames[k]);
    }
    return 1;
}
