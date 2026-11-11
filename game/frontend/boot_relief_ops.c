/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see boot_relief_ops.h. */
#include "game/frontend/boot_relief_ops.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* a running box along one line of n values `step` apart, the ends repeated */
static void box_line(const double *in, double *out, int n, int step, int r, double *line)
{
    for (int i = 0; i < n; i++) line[i] = in[(size_t)i * step];
    double sum = 0;
    for (int k = -r; k <= r; k++) sum += line[clampi(k, 0, n - 1)];
    for (int i = 0; i < n; i++) {
        out[(size_t)i * step] = sum / (2 * r + 1);
        sum += line[clampi(i + r + 1, 0, n - 1)] - line[clampi(i - r, 0, n - 1)];
    }
}

int ops_hbox(Plane p, const double *x, int r, double *out)
{
    double *line = malloc(sizeof(double) * (size_t)p.w);
    if (line == NULL) return 0;
    for (int y = 0; y < p.h; y++) box_line(x + (size_t)y * p.w, out + (size_t)y * p.w, p.w, 1, r, line);
    free(line);
    return 1;
}

/* down the columns, a whole row at a time (a column walk misses the cache): a running sum of rows */
int ops_vbox(Plane p, const double *x, int r, double *out)
{
    const size_t w = (size_t)p.w;
    double *in = malloc(sizeof(double) * w * p.h), *acc = calloc(w, sizeof(double));
    if (in == NULL || acc == NULL) {
        free(in), free(acc);
        return 0;
    }
    memcpy(in, x, sizeof(double) * w * p.h);   /* out may be x */
    for (int k = -r; k <= r; k++) {
        const double *row = in + (size_t)clampi(k, 0, p.h - 1) * w;
        for (size_t c = 0; c < w; c++) acc[c] += row[c];
    }
    for (int y = 0; y < p.h; y++) {
        double *o = out + (size_t)y * w;
        const double *add = in + (size_t)clampi(y + r + 1, 0, p.h - 1) * w, *sub = in + (size_t)clampi(y - r, 0, p.h - 1) * w;
        for (size_t c = 0; c < w; c++) o[c] = acc[c] / (2 * r + 1), acc[c] += add[c] - sub[c];
    }
    free(in), free(acc);
    return 1;
}

int ops_blur(Plane p, const double *x, int r, double *out)
{
    return ops_hbox(p, x, r, out) && ops_vbox(p, out, r, out);
}

int ops_nblur(Plane p, const double *x, const uint8_t *w, int r, double *out)
{
    const size_t n = (size_t)p.w * p.h;
    double *xw = malloc(sizeof(double) * n), *ww = malloc(sizeof(double) * n);
    int ok = xw != NULL && ww != NULL;
    if (ok) {
        for (size_t i = 0; i < n; i++) ww[i] = w[i] ? 1.0 : 0.0, xw[i] = w[i] ? x[i] : 0.0;
        ok = ops_blur(p, xw, r, xw) && ops_blur(p, ww, r, ww);
    }
    if (ok)
        for (size_t i = 0; i < n; i++) out[i] = xw[i] / fmax(ww[i], 1e-6);
    free(xw), free(ww);
    return ok;
}

/* a set pixel within r along a line of n values `step` apart: the distance since the last set one, both ways */
static void grow_line(const uint8_t *in, uint8_t *out, int n, int step, int r)
{
    int since = r + 1;
    for (int i = 0; i < n; i++) {
        since = in[(size_t)i * step] ? 0 : since + 1;
        out[(size_t)i * step] = since <= r;
    }
    since = r + 1;
    for (int i = n - 1; i >= 0; i--) {
        since = in[(size_t)i * step] ? 0 : since + 1;
        out[(size_t)i * step] |= since <= r;
    }
}

void ops_grow(Plane p, const uint8_t *m, int r, uint8_t *out)
{
    uint8_t *rows = malloc((size_t)p.w * p.h);
    if (rows == NULL) {   /* the slow way */
        for (int y = 0; y < p.h; y++)
            for (int x = 0; x < p.w; x++) {
                uint8_t any = 0;
                for (int dy = -r; dy <= r && !any; dy++)
                    for (int dx = -r; dx <= r && !any; dx++) any = m[(size_t)clampi(y + dy, 0, p.h - 1) * p.w + clampi(x + dx, 0, p.w - 1)];
                out[(size_t)y * p.w + x] = any;
            }
    } else {
        for (int y = 0; y < p.h; y++) grow_line(m + (size_t)y * p.w, rows + (size_t)y * p.w, p.w, 1, r);
        for (int x = 0; x < p.w; x++) grow_line(rows + x, out + x, p.h, p.w, r);
    }
    free(rows);
}

/* Jacobi steps on the pixels of `mask`: each the mean of its four neighbours, an edge neighbour the pixel itself */
static int relax(Plane p, double *x, const uint8_t *mask, int steps)
{
    size_t count = 0;
    for (size_t i = 0; i < (size_t)p.w * p.h; i++) count += mask[i] != 0;
    int *at = malloc(sizeof(int) * 5 * (count + 1));   /* each pixel and its four neighbours (an edge one the pixel itself) */
    double *next = malloc(sizeof(double) * (count + 1));
    if (at == NULL || next == NULL) {
        free(at), free(next);
        return 0;
    }
    count = 0;
    for (int i = 0; i < p.w * p.h; i++) {
        if (!mask[i]) continue;
        const int y = i / p.w, c = i % p.w;
        int *n = at + 5 * count++;
        n[0] = i, n[1] = y > 0 ? i - p.w : i, n[2] = y < p.h - 1 ? i + p.w : i, n[3] = c > 0 ? i - 1 : i, n[4] = c < p.w - 1 ? i + 1 : i;
    }
    for (int s = 0; s < steps; s++) {
        for (size_t k = 0; k < count; k++) {
            const int *n = at + 5 * k;
            next[k] = (x[n[1]] + x[n[2]] + x[n[3]] + x[n[4]]) / 4;
        }
        for (size_t k = 0; k < count; k++) x[at[5 * k]] = next[k];
    }
    free(at), free(next);
    return 1;
}

enum { MEMBRANE_BLOCK = 8, MEMBRANE_COARSE_STEPS = 800, MEMBRANE_FINE_STEPS = 400 };

int ops_membrane(Plane p, double *x, const uint8_t *mask)
{
    const int f = MEMBRANE_BLOCK, cw = p.w / f, ch = p.h / f;
    const size_t n = (size_t)p.w * p.h;
    double mean = 0;
    size_t outside = 0;
    for (size_t i = 0; i < n; i++)
        if (!mask[i]) mean += x[i], outside++;
    mean = outside > 0 ? mean / (double)outside : 0;
    for (size_t i = 0; i < n; i++)
        if (mask[i]) x[i] = mean;
    double *xc = calloc((size_t)cw * ch, sizeof(double));
    uint8_t *mc = calloc((size_t)cw * ch, 1);
    int ok = xc != NULL && mc != NULL;
    if (ok) {
        for (int y = 0; y < p.h; y++)
            for (int c = 0; c < p.w; c++) {
                const size_t b = (size_t)(y / f) * cw + c / f;
                xc[b] += x[(size_t)y * p.w + c] / (f * f);
                mc[b] += mask[(size_t)y * p.w + c] != 0;
            }
        for (size_t b = 0; b < (size_t)cw * ch; b++) mc[b] = mc[b] * 2 > f * f;
        ok = relax((Plane){ cw, ch }, xc, mc, MEMBRANE_COARSE_STEPS);
    }
    if (ok) {
        for (int y = 0; y < p.h; y++)
            for (int c = 0; c < p.w; c++)
                if (mask[(size_t)y * p.w + c]) x[(size_t)y * p.w + c] = xc[(size_t)(y / f) * cw + c / f];
        ok = relax(p, x, mask, MEMBRANE_FINE_STEPS);
    }
    free(xc), free(mc);
    return ok;
}

/* ---- the screened Poisson solve: multigrid V-cycles, cells merged 2 x 2 per level ---- */

enum { MG_LEVELS_MAX = 12, MG_PRE = 3, MG_POST = 3, MG_BOTTOM_SWEEPS = 4000, MG_CYCLES = 40 };

typedef struct {
    int w, h;
    double c;   /* 1 / spacing^2 */
    double *u, *f, *r;
} Level;

/* red-black Gauss-Seidel sweeps of (lambda - c * laplacian) u = f */
static void smooth(Level *l, double lambda, int sweeps)
{
    for (int s = 0; s < sweeps; s++)
        for (int colour = 0; colour < 2; colour++)
            for (int y = 0; y < l->h; y++)
                for (int x = (y + colour) & 1; x < l->w; x += 2) {
                    const size_t i = (size_t)y * l->w + x;
                    double sum = 0;
                    int k = 0;
                    if (x > 0) sum += l->u[i - 1], k++;
                    if (x < l->w - 1) sum += l->u[i + 1], k++;
                    if (y > 0) sum += l->u[i - l->w], k++;
                    if (y < l->h - 1) sum += l->u[i + l->w], k++;
                    l->u[i] = (l->f[i] + l->c * sum) / (lambda + l->c * k);
                }
}

static double residual(Level *l, double lambda)
{
    double worst = 0;
    for (int y = 0; y < l->h; y++)
        for (int x = 0; x < l->w; x++) {
            const size_t i = (size_t)y * l->w + x;
            double lap = 0;
            if (x > 0) lap += l->u[i - 1] - l->u[i];
            if (x < l->w - 1) lap += l->u[i + 1] - l->u[i];
            if (y > 0) lap += l->u[i - l->w] - l->u[i];
            if (y < l->h - 1) lap += l->u[i + l->w] - l->u[i];
            l->r[i] = l->f[i] - (lambda * l->u[i] - l->c * lap);
            worst = fmax(worst, fabs(l->r[i]));
        }
    return worst;
}

static void vcycle(Level *lv, int depth, int at, double lambda)
{
    Level *l = &lv[at];
    if (at == depth - 1) {
        smooth(l, lambda, MG_BOTTOM_SWEEPS);
        return;
    }
    Level *c = &lv[at + 1];
    smooth(l, lambda, MG_PRE);
    (void)residual(l, lambda);
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++) {
            const size_t i = (size_t)(2 * y) * l->w + 2 * x;
            c->f[(size_t)y * c->w + x] = (l->r[i] + l->r[i + 1] + l->r[i + l->w] + l->r[i + l->w + 1]) / 4;
            c->u[(size_t)y * c->w + x] = 0;
        }
    vcycle(lv, depth, at + 1, lambda);
    for (int y = 0; y < l->h; y++)
        for (int x = 0; x < l->w; x++) l->u[(size_t)y * l->w + x] += c->u[(size_t)(y / 2) * c->w + x / 2];
    smooth(l, lambda, MG_POST);
}

int ops_screened_poisson(Plane p, double lambda, const double *f, double *u)
{
    Level lv[MG_LEVELS_MAX];
    int depth = 0, ok = 1;
    for (int w = p.w, h = p.h, s = 1; depth < MG_LEVELS_MAX; w /= 2, h /= 2, s *= 2) {
        Level *l = &lv[depth++];
        l->w = w, l->h = h, l->c = 1.0 / ((double)s * s);
        l->u = depth == 1 ? u : calloc((size_t)w * h, sizeof(double));
        l->f = depth == 1 ? (double *)f : calloc((size_t)w * h, sizeof(double));
        l->r = calloc((size_t)w * h, sizeof(double));
        ok = ok && l->u != NULL && l->f != NULL && l->r != NULL;
        if (w % 2 != 0 || h % 2 != 0 || w < 8 || h < 8) break;
    }
    if (ok) {
        double scale = 0;
        for (size_t i = 0; i < (size_t)p.w * p.h; i++) scale = fmax(scale, fabs(f[i]));
        for (int cycle = 0; cycle < MG_CYCLES; cycle++) {
            vcycle(lv, depth, 0, lambda);
            if (residual(&lv[0], lambda) <= 1e-7 * fmax(scale, 1.0)) break;
        }
    }
    for (int i = 0; i < depth; i++) {
        if (i > 0) free(lv[i].u), free(lv[i].f);
        free(lv[i].r);
    }
    return ok;
}

void ops_noise(uint32_t seed, double *out, int n)
{
    uint32_t x = seed;
    for (int i = 0; i < n; i++) {
        double s = 0;
        for (int k = 0; k < 4; k++) {
            x ^= x << 13, x ^= x >> 17, x ^= x << 5;
            s += (x >> 8) / 16777216.0;
        }
        out[i] = (s - 2.0) * 1.7320508075688772;
    }
}
