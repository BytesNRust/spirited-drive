/* The car's four wheel frames from one picture (core/formats/car_wheels_gen.h): on a synthetic wheel of 5 spokes inside a treaded tyre,
 * the still frame is the picture, the slow frames are it turned by -/+ half the slow angle and lightly blurred, the fast one blurred
 * along the turn; the tyre band and the alpha never change; the bytes are the same on every run and machine. */
#include "formats/car_wheels_gen.h"
#include "test.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { SIZE = 64, SPOKES = 5 };
#define WHEEL_FRAMES_GOLDEN 0x0u   /* the slow and fast frames of the synthetic wheel, pinned: the same on every machine */
static const double PI = 3.14159265358979323846;

/* a wheel of radius 30 about (cx, cy) on a w x h picture: tread (alternating 30 / 70 every 15 degrees) past 0.7 R, spokes 220 on 100 inside */
static uint8_t *make_wheel(int w, int h, double cx, double cy)
{
    uint8_t *rgba = calloc((size_t)w * (size_t)h, 4);
    if (rgba == NULL) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const double dx = x + 0.5 - cx, dy = y + 0.5 - cy, r = sqrt(dx * dx + dy * dy);
            if (r > 30.0) continue;
            const double a = atan2(dy, dx) * 180.0 / PI + 360.0;
            uint8_t v;
            if (r > 21.0) v = ((int)(a / 15.0) % 2) ? 70 : 30;
            else if (r < 4.0) v = 180;
            else v = fmod(a, 360.0 / SPOKES) < 18.0 ? 220 : 100;
            uint8_t *p = rgba + 4 * ((size_t)y * (size_t)w + (size_t)x);
            p[0] = v, p[1] = (uint8_t)(v / 2), p[2] = (uint8_t)(255 - v), p[3] = 255;
        }
    return rgba;
}

static double lum(const uint8_t *p)
{
    return 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
}

/* the spokes' turn in the band r 8..18: the phase of the SPOKES-th harmonic of the luminance, in degrees, as a turn of the wheel */
static double spoke_phase(const uint8_t *rgba, int w, int h, double cx, double cy)
{
    double c = 0, s = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const double dx = x + 0.5 - cx, dy = y + 0.5 - cy, r = sqrt(dx * dx + dy * dy);
            if (r < 8.0 || r > 18.0) continue;
            const double a = atan2(dy, dx), l = lum(rgba + 4 * ((size_t)y * (size_t)w + (size_t)x));
            c += l * cos(SPOKES * a), s += l * sin(SPOKES * a);
        }
    return atan2(s, c) * 180.0 / PI / SPOKES;
}

/* the angular variance of the luminance in the band r 8..18 (the sum over rings 1 px wide of the variance around the ring) and each ring's mean */
static double band_variance(const uint8_t *rgba, int w, int h, double cx, double cy, double means[11])
{
    double sum[11] = { 0 }, sq[11] = { 0 };
    int n[11] = { 0 };
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const double dx = x + 0.5 - cx, dy = y + 0.5 - cy, r = sqrt(dx * dx + dy * dy);
            if (r < 8.0 || r >= 19.0) continue;
            const int k = (int)r - 8;
            const double l = lum(rgba + 4 * ((size_t)y * (size_t)w + (size_t)x));
            sum[k] += l, sq[k] += l * l, n[k]++;
        }
    double v = 0;
    for (int k = 0; k < 11; k++) {
        means[k] = n[k] ? sum[k] / n[k] : 0;
        if (n[k]) v += sq[k] / n[k] - means[k] * means[k];
    }
    return v;
}

static uint32_t crc32_of(const uint8_t *p, size_t n)
{
    uint32_t c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
    }
    return ~c;
}

static void test_defaults(void)
{
    WheelFrameOptions o;
    wheel_frame_options_default(&o);
    CHECK(o.tyre_share > 0.31 && o.tyre_share < 0.33);
    CHECK(o.slow_angle_deg > 19.9 && o.slow_angle_deg < 20.1);
    CHECK(o.fast_arc_deg > 0.0 && o.fast_arc_deg < 360.0);
    CHECK(!o.has_centre && !o.has_radius);
}

static void test_the_circle_is_found(void)
{
    uint8_t *rgba = make_wheel(80, 70, 41.0, 33.0);   /* off the picture's middle */
    CHECK(rgba != NULL);
    if (rgba == NULL) return;
    double cx = 0, cy = 0, r = 0;
    ExchangeError err = { { 0 } };
    CHECK(wheel_frame_find_circle(rgba, 80, 70, &cx, &cy, &r, &err));
    CHECK(fabs(cx - 41.0) < 0.6 && fabs(cy - 33.0) < 0.6 && fabs(r - 30.0) < 1.0);
    memset(rgba, 0, (size_t)80 * 70 * 4);   /* nothing opaque: no wheel */
    CHECK(!wheel_frame_find_circle(rgba, 80, 70, &cx, &cy, &r, &err) && err.message[0] != '\0');
    free(rgba);
}

static void test_the_four_frames(void)
{
    uint8_t *src = make_wheel(SIZE, SIZE, 32.0, 32.0);
    CHECK(src != NULL);
    if (src == NULL) return;
    WheelFrameOptions o;
    wheel_frame_options_default(&o);
    uint8_t *f[4] = { NULL, NULL, NULL, NULL };
    ExchangeError err = { { 0 } };
    CHECK(wheel_frames_make(src, SIZE, SIZE, &o, f, &err));
    if (f[0] == NULL || f[3] == NULL) { free(src); return; }
    const size_t bytes = (size_t)SIZE * SIZE * 4;
    CHECK(memcmp(f[0], src, bytes) == 0);   /* still: the picture */
    int alpha_same = 1, tyre_same = 1;
    for (int k = 0; k < 4; k++)
        for (int y = 0; y < SIZE; y++)
            for (int x = 0; x < SIZE; x++) {
                const size_t i = 4 * ((size_t)y * SIZE + (size_t)x);
                const double dx = x + 0.5 - 32.0, dy = y + 0.5 - 32.0, r = sqrt(dx * dx + dy * dy);
                alpha_same &= f[k][i + 3] == src[i + 3];
                if (r > 30.0 * (1.0 - o.tyre_share) + 0.75 && r <= 30.0) tyre_same &= memcmp(f[k] + i, src + i, 4) == 0;
            }
    CHECK(alpha_same);   /* the alpha is the picture's in every frame */
    CHECK(tyre_same);    /* the tyre band is never turned nor blurred */
    const double p0 = spoke_phase(src, SIZE, SIZE, 32, 32), p1 = spoke_phase(f[1], SIZE, SIZE, 32, 32), p2 = spoke_phase(f[2], SIZE, SIZE, 32, 32),
                 p3 = spoke_phase(f[3], SIZE, SIZE, 32, 32);
    fprintf(stderr, "spoke phases %.2f %.2f %.2f %.2f\n", p0, p1, p2, p3);
    CHECK(fabs((p1 - p0) - -10.0) < 1.5);   /* slow 1: turned by -10 degrees (half the slow angle) */
    CHECK(fabs((p2 - p0) - 10.0) < 1.5);    /* slow 2: by +10 */
    CHECK(fabs(p3 - p0) < 1.5);             /* fast: blurred about the picture's own angle */
    double m0[11], m1[11], m3[11];
    const double v0 = band_variance(src, SIZE, SIZE, 32, 32, m0), v1 = band_variance(f[1], SIZE, SIZE, 32, 32, m1), v3 = band_variance(f[3], SIZE, SIZE, 32, 32, m3);
    fprintf(stderr, "angular variance %.1f slow %.2f fast %.2f of it\n", v0, v1 / v0, v3 / v0);
    CHECK(v1 < 0.97 * v0 && v1 > 0.4 * v0);   /* slow: lightly blurred */
    CHECK(v3 < 0.5 * v0);                     /* fast: strongly */
    CHECK(v3 < v1);
    int profile = 1;
    for (int k = 0; k < 11; k++) profile &= fabs(m3[k] - m0[k]) < 4.0 && fabs(m1[k] - m0[k]) < 4.0;
    CHECK(profile);   /* the radial profile is kept: each ring's mean */
    /* the same bytes on every run (and on every machine: integer arithmetic only, pinned) */
    uint8_t *g[4] = { NULL, NULL, NULL, NULL };
    CHECK(wheel_frames_make(src, SIZE, SIZE, &o, g, &err));
    for (int k = 0; k < 4; k++) CHECK(g[k] != NULL && memcmp(f[k], g[k], bytes) == 0);
    uint32_t crc = 0;
    for (int k = 1; k < 4; k++) crc ^= crc32_of(f[k], bytes) * (uint32_t)k;
    fprintf(stderr, "frames crc %08x\n", crc);
    CHECK_EQ_INT(crc, WHEEL_FRAMES_GOLDEN);
    for (int k = 0; k < 4; k++) free(f[k]), free(g[k]);

    o.fast_arc_deg = 360.0;   /* a whole turn: nothing angular is left in the spokes' band */
    CHECK(wheel_frames_make(src, SIZE, SIZE, &o, f, &err));
    if (f[3] != NULL) {
        const double vf = band_variance(f[3], SIZE, SIZE, 32, 32, m3);
        fprintf(stderr, "whole turn: %.4f of the variance\n", vf / v0);
        CHECK(vf < 0.02 * v0);
    }
    for (int k = 0; k < 4; k++) free(f[k]);
    free(src);
}

static void test_refusals(void)
{
    uint8_t *src = make_wheel(SIZE, SIZE, 32.0, 32.0);
    CHECK(src != NULL);
    if (src == NULL) return;
    WheelFrameOptions o;
    uint8_t *f[4] = { NULL, NULL, NULL, NULL };
    ExchangeError err = { { 0 } };
    wheel_frame_options_default(&o);
    o.tyre_share = 1.0;
    CHECK(!wheel_frames_make(src, SIZE, SIZE, &o, f, &err) && strstr(err.message, "tyre") != NULL && f[0] == NULL);
    wheel_frame_options_default(&o);
    o.has_radius = 1, o.radius = 0.0;
    CHECK(!wheel_frames_make(src, SIZE, SIZE, &o, f, &err) && strstr(err.message, "radius") != NULL);
    wheel_frame_options_default(&o);
    o.slow_angle_deg = 400.0;
    CHECK(!wheel_frames_make(src, SIZE, SIZE, &o, f, &err));
    wheel_frame_options_default(&o);
    CHECK(!wheel_frames_make(src, 0, SIZE, &o, f, &err));
    free(src);
}

int main(void)
{
    test_defaults();
    test_the_circle_is_found();
    test_the_four_frames();
    test_refusals();
    return TEST_REPORT();
}
