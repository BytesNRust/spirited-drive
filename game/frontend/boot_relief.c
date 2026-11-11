/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see boot_relief.h. The steps and their numbers are the approved reference's (i030 out7 r5, the script on mock/i030-boot-credits). */
#include "game/frontend/boot_relief.h"

#include "game/frontend/boot_relief_ops.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { W = BOOT_RELIEF_W, H = BOOT_RELIEF_H, N = W * H, PLANES = 16, THIN_RUN = 24, RES_CLIP = 40, GRAIN_SEED = 7 };
static const double LAMBDA = 5e-4, GRAIN = 0.3, PHOTO_GREY = 0.15, DARK = 24.0, LIGHT = 12.0;
static const Plane P = { W, H };

const char *const BOOT_RELIEF_SCREEN_NAMES[BOOT_RELIEF_SCREENS] = { "iart/instunin.qfs", "iart/instinst.qfs", "iart/instvido.qfs" };
const BootReliefPiece BOOT_RELIEF_PIECE[BOOT_MAP_PIECES] = {
    { 0, 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, { 2, 0, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 1, 0 } };

typedef struct {
    const uint8_t *map;
    uint8_t *fill, *patch, *known, *um, *tmp8;
    double *c[3], *res[3], *t, *tp, *l, *a, *b, *d, *e, *g;   /* c: the collage, res: each pixel's tint, t: its luminance, tp: the target, l: solved */
    double lut[256][3];
} Work;

static double lum(const double r, const double g, const double b) { return r * .30 + g * .59 + b * .11; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

/* the collage: TITLE, the pieces, and its luminance with the picture's grey in and the fill left 0 */
static void collage(Work *k, const BootReliefSources *in)
{
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int i = y * W + x, m = k->map[i];
            const uint8_t *s = in->title + 3 * i;
            if (m >= BOOT_MAP_PIECE) {
                const BootReliefPiece *pc = &BOOT_RELIEF_PIECE[m - BOOT_MAP_PIECE];
                s = in->screens[pc->screen] + 3 * (clampi(y + pc->dy, 0, H - 1) * W + clampi(x + pc->dx, 0, W - 1));
            }
            for (int ch = 0; ch < 3; ch++) k->c[ch][i] = s[ch];
            k->fill[i] = m == BOOT_MAP_FILL, k->patch[i] = m == BOOT_MAP_PHOTO, k->known[i] = !k->fill[i];
            k->t[i] = k->fill[i] ? 0 : lum(s[0], s[1], s[2]);
        }
    for (int y = 0; y < BOOT_PHOTO_H; y++)
        for (int x = 0; x < BOOT_PHOTO_W; x++) {
            const int i = (BOOT_PHOTO_Y + y) * W + BOOT_PHOTO_X + x;
            const uint8_t *s = in->photo + 3 * (y * BOOT_PHOTO_W + x);
            if (k->patch[i]) k->t[i] = lum(s[0], s[1], s[2]);
        }
}

static double mean_over(const double *x, const uint8_t *m, int square)
{
    double s = 0;
    int n = 0;
    for (int i = 0; i < N; i++)
        if (m[i]) s += square ? x[i] * x[i] : x[i], n++;
    return n > 0 ? s / n : 0;
}

/* the picture as relief: its grey embossed (light from the top left) at its neighbours' contrast, plus a little of its grey. tp = t outside. */
static int photo_relief(Work *k)
{
    double *g = k->g, *e = k->e, *hp = k->a, *eb = k->b;
    for (int i = 0; i < N; i++) k->d[i] = k->patch[i] ? k->t[i] : 0;
    if (!ops_nblur(P, k->d, k->patch, 2, g)) return 0;
    for (int i = 0; i < N; i++)
        if (k->patch[i]) g[i] = k->t[i];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) e[y * W + x] = g[((y + H - 1) % H) * W + (x + W - 1) % W] - g[((y + 1) % H) * W + (x + 1) % W];
    if (!ops_nblur(P, k->t, k->known, 3, hp) || !ops_blur(P, e, 3, eb)) return 0;
    for (int i = 0; i < N; i++) hp[i] = k->t[i] - hp[i], eb[i] = e[i] - eb[i];
    ops_grow(P, k->patch, 10, k->tmp8);
    for (int i = 0; i < N; i++) k->tmp8[i] = k->tmp8[i] && !k->patch[i] && k->known[i];   /* the ring round it */
    const double gain = sqrt(mean_over(hp, k->tmp8, 1)) / fmax(sqrt(mean_over(eb, k->patch, 1)), 1e-6);
    const double level = mean_over(k->t, k->tmp8, 0), grey = mean_over(g, k->patch, 0);
    for (int i = 0; i < N; i++) k->tp[i] = k->patch[i] ? level + gain * e[i] + PHOTO_GREY * (g[i] - grey) : k->t[i];
    return 1;   /* k->a keeps the high pass, for the grain */
}

/* the fill: the edge's level and a calm grain at its neighbours' contrast; thin runs mirrored from above and below */
static int fill_in(Work *k)
{
    double *hp = k->a, *lvl = k->b, *sd = k->d, *grain = k->e;
    for (int i = 0; i < N; i++) k->g[i] = k->known[i] ? hp[i] * hp[i] : 0;
    if (!ops_nblur(P, k->g, k->known, 4, sd) || !ops_nblur(P, k->t, k->known, 2, lvl)) return 0;
    for (int i = 0; i < N; i++) sd[i] = k->fill[i] ? 0 : sqrt(fmax(sd[i], 0)), lvl[i] = k->fill[i] ? 0 : lvl[i];
    if (!ops_membrane(P, lvl, k->fill) || !ops_membrane(P, sd, k->fill)) return 0;
    ops_noise(GRAIN_SEED, grain, N);
    if (!ops_blur(P, grain, 1, k->g)) return 0;
    for (int i = 0; i < N; i++)
        if (k->fill[i]) k->tp[i] = lvl[i] + GRAIN * sd[i] * (grain[i] - 0.5 * k->g[i]);
    for (int x = 0; x < W; x++)
        for (int y = 0; y < H;) {
            if (!k->fill[y * W + x]) {
                y++;
                continue;
            }
            const int y0 = y;
            while (y < H && k->fill[y * W + x]) y++;
            const int n = y - y0;
            if (n > THIN_RUN || y0 == 0 || y >= H) continue;
            for (int j = 0; j < n; j++) {
                const double up = k->tp[clampi(y0 - 1 - j, 0, H - 1) * W + x], dn = k->tp[clampi(y + n - 1 - j, 0, H - 1) * W + x], w = (j + 0.5) / n;
                k->tp[(y0 + j) * W + x] = (1 - w) * up + w * dn;
            }
        }
    return 1;
}

/* the seams: the target's gradients, dropped between regions, solved again */
static int solve(Work *k)
{
    const uint8_t *m = k->map;
    double *gx = k->a, *gy = k->b, *f = k->d;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int i = y * W + x;
            gx[i] = x < W - 1 && m[i] == m[i + 1] ? k->tp[i + 1] - k->tp[i] : 0;
            gy[i] = y < H - 1 && m[i] == m[i + W] ? k->tp[i + W] - k->tp[i] : 0;
        }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int i = y * W + x;
            const double div = gx[i] - (x > 0 ? gx[i - 1] : 0) + gy[i] - (y > 0 ? gy[i - W] : 0);
            f[i] = LAMBDA * k->tp[i] - div;
            k->l[i] = k->tp[i];
        }
    return ops_screened_poisson(P, LAMBDA, f, k->l);
}

static int cmp_double(const void *a, const void *b)
{
    const double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* the collage's luminance -> colour table: per level the median colour of its known, non-picture pixels (30 at least), filled across, smoothed by 5 */
static int build_lut(Work *k)
{
    int count[256] = { 0 }, start[257] = { 0 };
    for (int i = 0; i < N; i++)
        if (k->known[i] && !k->patch[i]) count[(int)clampd(k->t[i], 0, 255)]++;
    for (int v = 0; v < 256; v++) start[v + 1] = start[v] + count[v];
    double *vals = malloc(sizeof(double) * N);
    int *order = malloc(sizeof(int) * N), at[256];
    if (vals == NULL || order == NULL) {
        free(vals), free(order);
        return 0;
    }
    memcpy(at, start, sizeof at);
    for (int i = 0; i < N; i++)   /* the pixels by level */
        if (k->known[i] && !k->patch[i]) order[at[(int)clampd(k->t[i], 0, 255)]++] = i;
    int have[256], nh = 0;
    for (int v = 0; v < 256; v++) {
        if (count[v] < 30) continue;
        for (int ch = 0; ch < 3; ch++) {
            const int n = count[v];
            for (int j = 0; j < n; j++) vals[j] = k->c[ch][order[start[v] + j]];
            qsort(vals, (size_t)n, sizeof(double), cmp_double);
            k->lut[v][ch] = (vals[(n - 1) / 2] + vals[n / 2]) / 2;
        }
        have[nh++] = v;
    }
    free(vals), free(order);
    if (nh == 0) return 0;
    double raw[256][3];
    for (int v = 0, j = 0; v < 256; v++) {
        while (j < nh - 1 && have[j + 1] <= v) j++;
        for (int ch = 0; ch < 3; ch++) {
            const int a = have[j], b = have[j < nh - 1 ? j + 1 : j];
            raw[v][ch] = v <= have[0] ? k->lut[have[0]][ch] : v >= have[nh - 1] ? k->lut[have[nh - 1]][ch]
                       : k->lut[a][ch] + (k->lut[b][ch] - k->lut[a][ch]) * (v - a) / (double)(b - a);
        }
    }
    for (int v = 0; v < 256; v++)
        for (int ch = 0; ch < 3; ch++) {
            double s = 0;
            for (int d = -2; d <= 2; d++) s += raw[clampi(v + d, 0, 255)][ch];
            k->lut[v][ch] = s / 5;
        }
    return 1;
}

static const double *look(const Work *k, double v) { return k->lut[(int)clampd(v, 0, 255)]; }

/* each pixel's tint against the table; over the picture and the fill carried in from their edge */
static int tints(Work *k)
{
    ops_grow(P, k->patch, 1, k->um);
    for (int i = 0; i < N; i++) k->um[i] = k->um[i] || k->fill[i], k->tmp8[i] = !k->um[i];
    for (int ch = 0; ch < 3; ch++) {
        double *r = k->res[ch];
        for (int i = 0; i < N; i++) r[i] = clampd(k->c[ch][i] - look(k, k->t[i])[ch], -RES_CLIP, RES_CLIP);
        if (!ops_nblur(P, r, k->tmp8, 3, k->a)) return 0;
        for (int i = 0; i < N; i++) k->a[i] = k->um[i] ? 0 : k->a[i];
        if (!ops_membrane(P, k->a, k->um)) return 0;
        for (int i = 0; i < N; i++)
            if (k->um[i]) r[i] = k->a[i];
    }
    return 1;
}

/* X += weight * (the low pass smoothed along rows (across) or columns (down) - the low pass) */
static int smooth_band(double *x, const double *weight, int low_r, int box_r, int across, double *low, double *tmp)
{
    if (!ops_blur(P, x, low_r, low)) return 0;
    int ok = across ? ops_hbox(P, low, box_r, tmp) && ops_hbox(P, tmp, box_r, tmp) : ops_vbox(P, low, box_r, tmp) && ops_vbox(P, tmp, box_r, tmp);
    if (ok)
        for (int i = 0; i < N; i++) x[i] += weight[i] * (tmp[i] - low[i]);
    return ok;
}

/* the owner's marks on out7: the top band (x < 350, y < 62) smoothed across with its vertical seams cross-faded; the title-rule band smoothed down with
 * its horizontal seams cross-faded; the fill darker at the top and lighter under the banner */
static int marks(Work *k)
{
    double *wb = k->g, *tri = k->e, *low = k->a, *tmp = k->b;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) wb[y * W + x] = clampd((62.0 - y) / 14, 0, 1) * clampd((350.0 - x) / 20, 0, 1);
    for (int pass = 0; pass < 2; pass++) {
        const int across = pass == 0;
        if (!across)
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) wb[y * W + x] = clampd((x - 170.0) / 20, 0, 1) * clampd(1 - fabs(y - 63.0) / 40, 0, 1);
        if (!smooth_band(k->l, wb, across ? 6 : 4, across ? 40 : 22, across, low, tmp)) return 0;
        for (int ch = 0; ch < 3; ch++)
            if (!smooth_band(k->res[ch], wb, across ? 6 : 4, across ? 40 : 22, across, low, tmp)) return 0;
        /* the cross-fade: 1 - (the distance to the nearest seam along the line) / (reach + 1), a seam counting where the weight is on */
        const int reach = across ? 12 : 6, lines = across ? H : W, len = across ? W : H, step = across ? 1 : W, next = across ? 1 : W;
        for (int l = 0; l < lines; l++) {
            const int base = across ? l * W : l;
            for (int dir = 0; dir < 2; dir++) {
                int since = reach + 1;
                for (int j = 0; j < len; j++) {
                    const int jj = dir == 0 ? j : len - 1 - j, s = base + jj * step;
                    since = wb[s] > 0 && jj < len - 1 && k->map[s] != k->map[s + next] ? 0 : since + 1;
                    const double t = since <= reach ? 1 - (double)since / (reach + 1) : 0;
                    tri[s] = dir == 0 ? t : fmax(tri[s], t);
                }
            }
            if (across)
                for (int j = 0; j < len; j++) tri[base + j] *= wb[base + j];
        }
        if (!ops_blur(P, k->l, 1, low)) return 0;
        if (!(across ? ops_hbox(P, low, 6, tmp) : ops_vbox(P, low, 3, tmp))) return 0;
        for (int i = 0; i < N; i++) k->l[i] = k->l[i] * (1 - tri[i]) + tri[i] * tmp[i];
    }
    for (int i = 0; i < N; i++) k->a[i] = k->fill[i];
    if (!ops_blur(P, k->a, 8, k->a)) return 0;
    for (int i = 0; i < N; i++) k->l[i] += (i / W < 120 ? -DARK : LIGHT) * k->a[i];
    return 1;
}

int boot_relief_compose(const BootReliefSources *in, uint8_t rgb[BOOT_RELIEF_W * BOOT_RELIEF_H * 3])
{
    for (int i = 0; i < N; i++)
        if (in->map[i] >= BOOT_MAP_PIECE + BOOT_MAP_PIECES) return 0;
    Work *k = calloc(1, sizeof *k);
    double *planes = malloc(sizeof(double) * N * PLANES);
    uint8_t *masks = malloc((size_t)N * 5);
    int ok = k != NULL && planes != NULL && masks != NULL;
    if (ok) {
        double **p[PLANES] = { &k->c[0], &k->c[1], &k->c[2], &k->res[0], &k->res[1], &k->res[2], &k->t, &k->tp, &k->l, &k->a, &k->b, &k->d, &k->e, &k->g };
        for (int i = 0; i < PLANES; i++)
            if (p[i] != NULL) *p[i] = planes + (size_t)i * N;
        k->fill = masks, k->patch = masks + N, k->known = masks + 2 * N, k->um = masks + 3 * N, k->tmp8 = masks + 4 * N;
        k->map = in->map;
        collage(k, in);
        ok = photo_relief(k) && fill_in(k) && solve(k) && build_lut(k) && tints(k) && marks(k);
    }
    if (ok)
        for (int i = 0; i < N; i++) {
            const double *c = look(k, k->l[i]);
            for (int ch = 0; ch < 3; ch++) rgb[3 * i + ch] = (uint8_t)clampd(c[ch] + k->res[ch][i], 0, 255);
        }
    free(planes), free(masks), free(k);
    return ok;
}
