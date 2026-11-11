/* The boot relief's image operations: box blurs with repeated edges, masked blurs, the membrane fill, the screened Poisson solve, the grain. */
#include "game/frontend/boot_relief_ops.h"
#include "test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int near(double a, double b, double eps) { return fabs(a - b) <= eps; }

static void the_blur_repeats_the_edge(void)
{
    const Plane p = { 4, 1 };
    const double x[4] = { 0, 3, 6, 9 };
    double out[4];
    CHECK(ops_blur(p, x, 1, out));
    /* 3 x 3 around (0,0) with the edge repeated: rows all the same, columns 0 0 3 */
    CHECK(near(out[0], 1.0, 1e-12));
    CHECK(near(out[1], 3.0, 1e-12));
    CHECK(near(out[3], 8.0, 1e-12));
}

static void the_masked_blur_reads_only_the_mask(void)
{
    const Plane p = { 3, 1 };
    const double x[3] = { 10, 1000, 30 };
    const uint8_t w[3] = { 1, 0, 1 };
    double out[3];
    CHECK(ops_nblur(p, x, w, 1, out));
    CHECK(near(out[1], 20.0, 1e-9));
    CHECK(near(out[0], 10.0, 1e-9));   /* its square: 10 (repeated) and 10, 1000 left out */
}

static void growing_dilates_by_a_square(void)
{
    const Plane p = { 5, 5 };
    uint8_t m[25] = { 0 }, out[25];
    m[12] = 1;
    ops_grow(p, m, 1, out);
    int n = 0;
    for (int i = 0; i < 25; i++) n += out[i];
    CHECK_EQ_INT(n, 9);
    CHECK(out[6] && out[18] && !out[0]);
}

static void the_row_and_column_boxes(void)
{
    const Plane p = { 3, 2 };
    const double x[6] = { 0, 3, 6, 0, 3, 6 };
    double h[6], v[6];
    CHECK(ops_hbox(p, x, 1, h));
    CHECK(ops_vbox(p, x, 1, v));
    CHECK(near(h[1], 3.0, 1e-12) && near(h[0], 1.0, 1e-12) && near(h[2], 5.0, 1e-12));
    CHECK(near(v[2], 6.0, 1e-12));
}

/* a linear ramp is harmonic: the membrane fills a hole in it with the ramp itself, near enough */
static void the_membrane_fills_a_ramp(void)
{
    const Plane p = { 64, 48 };
    double *x = malloc(sizeof(double) * 64 * 48);
    uint8_t *m = calloc(64 * 48, 1);
    for (int y = 0; y < 48; y++)
        for (int i = 0; i < 64; i++) x[y * 64 + i] = 2.0 * i;
    for (int y = 16; y < 32; y++)
        for (int i = 24; i < 40; i++) m[y * 64 + i] = 1, x[y * 64 + i] = -500;
    CHECK(ops_membrane(p, x, m));
    double worst = 0;
    for (int y = 16; y < 32; y++)
        for (int i = 24; i < 40; i++) worst = fmax(worst, fabs(x[y * 64 + i] - 2.0 * i));
    CHECK(worst < 1.0);
    free(x), free(m);
}

/* the solve against the operator applied back */
static void the_poisson_solve_inverts_the_operator(void)
{
    const int w = 64, h = 48;
    const Plane p = { w, h };
    const double lambda = 5e-4;
    double *u = calloc((size_t)w * h, sizeof(double)), *f = malloc(sizeof(double) * w * h), *want = malloc(sizeof(double) * w * h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) want[y * w + x] = 100 + 40 * sin(x / 7.0) + 30 * cos(y / 5.0) + (x > 30 ? 25 : 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const double c = want[y * w + x];
            double lap = 0;
            if (x > 0) lap += want[y * w + x - 1] - c;
            if (x < w - 1) lap += want[y * w + x + 1] - c;
            if (y > 0) lap += want[(y - 1) * w + x] - c;
            if (y < h - 1) lap += want[(y + 1) * w + x] - c;
            f[y * w + x] = lambda * c - lap;
        }
    CHECK(ops_screened_poisson(p, lambda, f, u));
    double worst = 0;
    for (int i = 0; i < w * h; i++) worst = fmax(worst, fabs(u[i] - want[i]));
    CHECK(worst < 0.05);
    free(u), free(f), free(want);
}

/* the same numbers as the reference script's xorshift (seed 7) */
static void the_grain_is_the_references(void)
{
    double n[3];
    ops_noise(7, n, 3);
    uint32_t x = 7;
    for (int i = 0; i < 3; i++) {
        double s = 0;
        for (int k = 0; k < 4; k++) {
            x ^= x << 13, x ^= x >> 17, x ^= x << 5;
            s += (x >> 8) / 16777216.0;
        }
        CHECK(near(n[i], (s - 2.0) * sqrt(3.0), 1e-12));
    }
    double *big = malloc(sizeof(double) * 100000), m = 0, v = 0;
    ops_noise(7, big, 100000);
    for (int i = 0; i < 100000; i++) m += big[i], v += big[i] * big[i];
    m /= 100000, v = v / 100000 - m * m;
    CHECK(fabs(m) < 0.02 && fabs(v - 1.0) < 0.03);
    free(big);
}

int main(void)
{
    the_blur_repeats_the_edge();
    the_masked_blur_reads_only_the_mask();
    growing_dilates_by_a_square();
    the_row_and_column_boxes();
    the_membrane_fills_a_ramp();
    the_poisson_solve_inverts_the_operator();
    the_grain_is_the_references();
    return TEST_REPORT();
}
