/* psx_light_fit <psx_dir> <data_dir> <A|B|C> <track>...: fits the PS1 CD's light of those tracks (at one time of day) from what our own generator can see of the terrain, numbers
 * only: for each terrain point (2..5, 7..10) and for all of them, the least-squares coefficients of the CD's byte / 255 over
 *   1, sky, sun_tan, nx, nz, ny, rise, lateral
 * (psx_light_make.h's PsxLightFeatures: the share of the dome seen, the horizon toward the default sun, the surface normal's x, z and y, how far over and across the
 * node's centre the point is), the R squared, and the mean absolute error of the fit in the 0..255 bytes. Also the mean of the CD's bytes per point. Reads the CD's and the PC's
 * files, writes nothing and prints no byte of them: only these statistics. Tunnel nodes are skipped. */
#include "game/race/psx_light.h"
#include "game/race/psx_light_make.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FEATURES = 8, MAX_SAMPLES = 400000 };
static const char *const NAME[FEATURES] = { "1", "sky", "sun_tan", "nx", "nz", "ny", "rise", "lateral" };

typedef struct {
    float x[FEATURES];
    float y;
    uint8_t point;
} Sample;

static Sample *samples;
static size_t sample_count;

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = n > 0 ? malloc((size_t)n) : NULL;
    if (data != NULL && fread(data, 1, (size_t)n, f) != (size_t)n) free(data), data = NULL;
    fclose(f);
    *size = data != NULL ? (size_t)n : 0;
    return data;
}

static int terrain(int p) { return p != 0 && p != 1 && p != 6; }

static int add_track(const char *psx_dir, const char *data_dir, const char *name, char letter)
{
    char track[16], path[512];
    size_t i = 0;
    for (; name[i] != '\0' && i < sizeof track - 1; i++) track[i] = (char)toupper((unsigned char)name[i]);
    track[i] = '\0';
    size_t tri_size, lgt_size;
    snprintf(path, sizeof path, "%s/SIMDATA/MISC/%s.TRI", data_dir, track);
    uint8_t *tri_data = slurp(path, &tri_size);
    static Tri tri;
    if (tri_data == NULL || tri_open(&tri, tri_data, tri_size) != TRI_OK) return fprintf(stderr, "cannot read %s\n", path), 0;
    snprintf(path, sizeof path, "%s/Z%s%c.LGT", psx_dir, track, letter);
    uint8_t *lgt_data = slurp(path, &lgt_size);
    PsxLight ps1;
    if (lgt_data == NULL || !psx_light_parse(lgt_data, lgt_size, tri.node_count, &ps1)) return fprintf(stderr, "cannot read %s\n", path), 0;
    const uint32_t count = tri.node_count;
    LightPos *pts = malloc((size_t)count * PSX_LIGHT_POINTS * sizeof *pts);
    if (pts == NULL || !psx_light_tri_points(&tri, pts)) return fprintf(stderr, "no terrain\n"), 0;
    const int ring = tri_is_circuit(&tri);
    for (uint32_t n = 0; n + 1 < count; n++) {
        if (psx_light_tunnel(&ps1, n)) continue;
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
            if (!terrain(p) || sample_count >= MAX_SAMPLES) continue;
            PsxLightFeatures f;
            psx_light_features(pts, count, ring, n, p, &f);
            Sample *s = &samples[sample_count++];
            const float x[FEATURES] = { 1, f.sky, f.sun_tan, f.nx, f.nz, f.ny, f.rise / 10.0f, f.lateral / 10.0f };
            memcpy(s->x, x, sizeof x);
            s->y = (float)psx_light_byte(&ps1, n, p) / 255.0f;
            s->point = (uint8_t)p;
        }
    }
    printf("  %s: %u nodes\n", track, count);
    psx_light_free(&ps1);
    free(pts), free(tri_data), free(lgt_data);
    return 1;
}

/* least squares by the normal equations over the samples of point `point` (-1: all) */
static int fit(int point, double coef[FEATURES], double *r2, double *mae, double *mean, long *n_out)
{
    double a[FEATURES][FEATURES + 1] = { { 0 } }, sy = 0, syy = 0;
    long n = 0;
    for (size_t k = 0; k < sample_count; k++) {
        const Sample *s = &samples[k];
        if (point >= 0 && s->point != point) continue;
        for (int i = 0; i < FEATURES; i++) {
            for (int j = 0; j < FEATURES; j++) a[i][j] += (double)s->x[i] * s->x[j];
            a[i][FEATURES] += (double)s->x[i] * s->y;
        }
        sy += s->y, syy += (double)s->y * s->y, n++;
    }
    *n_out = n;
    if (n < 50) return 0;
    for (int i = 0; i < FEATURES; i++) a[i][i] += 1e-6;   /* a ridge so a constant column does not make it singular */
    for (int c = 0; c < FEATURES; c++) {
        int pivot = c;
        for (int r = c + 1; r < FEATURES; r++)
            if (fabs(a[r][c]) > fabs(a[pivot][c])) pivot = r;
        for (int j = 0; j <= FEATURES; j++) { const double t = a[c][j]; a[c][j] = a[pivot][j]; a[pivot][j] = t; }
        if (fabs(a[c][c]) < 1e-12) return 0;
        for (int r = 0; r < FEATURES; r++) {
            if (r == c) continue;
            const double m = a[r][c] / a[c][c];
            for (int j = c; j <= FEATURES; j++) a[r][j] -= m * a[c][j];
        }
    }
    for (int i = 0; i < FEATURES; i++) coef[i] = a[i][FEATURES] / a[i][i];
    double sse = 0, abs_err = 0;
    for (size_t k = 0; k < sample_count; k++) {
        const Sample *s = &samples[k];
        if (point >= 0 && s->point != point) continue;
        double v = 0;
        for (int i = 0; i < FEATURES; i++) v += coef[i] * s->x[i];
        v = v < 0 ? 0 : v > 1 ? 1 : v;
        sse += (v - s->y) * (v - s->y), abs_err += fabs(v - s->y);
    }
    *mean = sy / (double)n;
    const double var = syy / (double)n - *mean * *mean;
    *r2 = var > 1e-9 ? 1.0 - sse / (double)n / var : 0;
    *mae = 255.0 * abs_err / (double)n;
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 5) return fprintf(stderr, "usage: psx_light_fit <psx_dir> <data_dir> <A|B|C> <track>...\n"), 2;
    const char letter = (char)toupper((unsigned char)argv[3][0]);
    samples = malloc(MAX_SAMPLES * sizeof *samples);
    if (samples == NULL) return 1;
    printf("psx_light_fit, time %c, tracks:\n", letter);
    for (int i = 4; i < argc; i++)
        if (!add_track(argv[1], argv[2], argv[i], letter)) return 1;
    printf("%zu samples\ncoefficients of byte/255 over:", sample_count);
    for (int i = 0; i < FEATURES; i++) printf(" %s", NAME[i]);
    printf("\n");
    for (int p = -1; p < PSX_LIGHT_POINTS; p++) {
        if (p >= 0 && !terrain(p)) continue;
        double coef[FEATURES], r2, mae, mean;
        long n;
        if (!fit(p, coef, &r2, &mae, &mean, &n)) continue;
        printf("%s%2d: n %6ld mean %5.1f R2 %.2f fitMAE %5.1f |", p < 0 ? "all" : "pt ", p < 0 ? 0 : p, n, mean * 255.0, r2, mae);
        for (int i = 0; i < FEATURES; i++) printf(" %+.3f", coef[i]);
        printf("\n");
    }
    free(samples);
    return 0;
}
