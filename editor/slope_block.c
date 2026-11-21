#include "slope_block.h"

#include <math.h>
#include <stdlib.h>

enum { STEPS_PER_SEGMENT = 64 };

static double smoothstep(double t)
{
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return t * t * (3 - 2 * t);
}

/* the two shapes the grade is made of: it is p(s) + G q(s) */
static void shapes(double s, double len, double ein, double eout, double g0, double g1, double *p, double *q)
{
    const double in = ein > 0 ? smoothstep(s / ein) : 1.0, out = eout > 0 ? smoothstep((len - s) / eout) : 1.0;
    *p = g0 * (1 - in) + g1 * (1 - out);
    *q = in + out - 1;
}

/* the integrals of p and q over [a, b] (Simpson over equal steps: the shapes are smooth but for the easings' ends, which a segment this short hardly feels) */
static void integrate(double a, double b, double len, double ein, double eout, double g0, double g1, double *ip, double *iq)
{
    const int n = 2 * STEPS_PER_SEGMENT;
    const double h = (b - a) / n;
    double sp = 0, sq = 0;
    for (int k = 0; k <= n; ++k) {
        double p, q;
        shapes(a + k * h, len, ein, eout, g0, g1, &p, &q);
        const double w = k == 0 || k == n ? 1 : k % 2 ? 4 : 2;
        sp += w * p;
        sq += w * q;
    }
    *ip = sp * h / 3;
    *iq = sq * h / 3;
}

int slope_block(const double *dist, int count, double start_h, double g0_pct, double g1_pct, int by_rise, double value, double ease_in_m, double ease_out_m, double *out,
                double *grade_pct)
{
    if (dist == NULL || out == NULL || count < 2) return 0;
    if (!isfinite(start_h) || !isfinite(g0_pct) || !isfinite(g1_pct) || !isfinite(value) || !isfinite(ease_in_m) || !isfinite(ease_out_m)) return 0;
    for (int i = 0; i < count; ++i)
        if (!isfinite(dist[i]) || (i > 0 && !(dist[i] > dist[i - 1]))) return 0;
    const double len = dist[count - 1] - dist[0];
    if (ease_in_m < 0 || ease_out_m < 0 || ease_in_m + ease_out_m > len) return 0;
    if (by_rise ? fabs(value) > SLOPE_RISE_MAX_M : fabs(value) > SLOPE_GRADE_MAX_PCT) return 0;
    double *ip = malloc(sizeof *ip * (size_t)(count - 1) * 2);
    if (ip == NULL) return 0;
    double *iq = ip + (count - 1);
    double tp = 0, tq = 0;
    for (int i = 0; i + 1 < count; ++i) {
        integrate(dist[i] - dist[0], dist[i + 1] - dist[0], len, ease_in_m, ease_out_m, g0_pct, g1_pct, &ip[i], &iq[i]);
        tp += ip[i];
        tq += iq[i];
    }
    const double grade = by_rise ? (100.0 * value - tp) / tq : value;   /* tq is at least half the run: the easings take at most all of it */
    if (!(fabs(grade) <= SLOPE_GRADE_MAX_PCT)) {   /* a rise that would need more than the largest grade */
        free(ip);
        return 0;
    }
    out[0] = start_h;
    for (int i = 0; i + 1 < count; ++i) out[i + 1] = out[i] + (ip[i] + grade * iq[i]) / 100.0;
    if (grade_pct != NULL) *grade_pct = grade;
    free(ip);
    return 1;
}
