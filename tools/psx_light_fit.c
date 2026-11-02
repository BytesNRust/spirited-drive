/* psx_light_fit <psx_dir> <data_dir> <A|B|C> <track>...: fits the PS1 CD's light of those tracks (at one time of day) from what our own generator can see of the terrain, numbers
 * only: for each terrain point (2..5, 7..10) and for all of them, the least-squares coefficients of the CD's byte / 255 over
 *   1, sky, nx, ny
 * (psx_light_make.h's PsxLightFeatures: the share of the dome seen, the surface normal's x and y; the PS1's sun is fixed along the track's x axis), the R squared, and the mean absolute error of the fit in the 0..255 bytes. Also the mean of the CD's bytes per point. Reads the CD's and the PC's
 * files, writes nothing and prints no byte of them: only these statistics. Tunnel nodes are skipped. */
#include "game/race/psx_light.h"
#include "game/race/psx_light_make.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FEATURES = 5, MAX_SAMPLES = 400000, MAX_TRACKS = 32 };
static const char *const NAME[FEATURES] = { "1", "sky", "nx", "nz", "ny" };   /* the first fit with all eight showed the others (sun_tan, nz, rise, lateral) near 0 */

typedef struct {
    float x[FEATURES];
    float y;
    uint8_t point;
} Sample;

static Sample *samples;
static size_t sample_count;
static struct { char name[16]; size_t first, end; double heading; } tracks[MAX_TRACKS];   /* a track's samples, and the heading of its first stretch (degrees) */
static int track_count;
static size_t fit_lo, fit_hi;   /* the samples a fit takes */
static int terrain_only_inner;   /* 1: the points 2..4 and 7..9, those that fit */

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
    const size_t first = sample_count;
    const uint32_t ahead = count > 12 ? 12 : count - 1;
    const double heading = atan2((double)(pts[ahead * PSX_LIGHT_POINTS].z - pts[0].z), (double)(pts[ahead * PSX_LIGHT_POINTS].x - pts[0].x)) * 180.0 / 3.14159265;
    for (uint32_t n = 0; n + 1 < count; n++) {
        if (psx_light_tunnel(&ps1, n)) continue;
        for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
            if (!terrain(p) || sample_count >= MAX_SAMPLES) continue;
            PsxLightFeatures f;
            psx_light_features(pts, count, ring, n, p, &f);
            Sample *s = &samples[sample_count++];
            const float x[FEATURES] = { 1, f.sky, f.nx, f.nz, f.ny };
            memcpy(s->x, x, sizeof x);
            s->y = (float)psx_light_byte(&ps1, n, p) / 255.0f;
            s->point = (uint8_t)p;
        }
    }
    if (track_count < MAX_TRACKS) {
        snprintf(tracks[track_count].name, sizeof tracks[track_count].name, "%s", track);
        tracks[track_count].first = first, tracks[track_count].end = sample_count, tracks[track_count].heading = heading;
        track_count++;
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
        if (k < fit_lo || k >= fit_hi || (point >= 0 && s->point != point) || (point == -2 && (s->point == 5 || s->point == 10))) continue;
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
        if (k < fit_lo || k >= fit_hi || (point >= 0 && s->point != point) || (point == -2 && (s->point == 5 || s->point == 10))) continue;
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
    fit_lo = 0, fit_hi = sample_count;
    for (int p = -1; p < PSX_LIGHT_POINTS; p++) {
        if (p >= 0 && !terrain(p)) continue;
        double coef[FEATURES], r2, mae, mean;
        long n;
        if (!fit(p, coef, &r2, &mae, &mean, &n)) continue;
        printf("%s%2d: n %6ld mean %5.1f R2 %.2f fitMAE %5.1f |", p < 0 ? "all" : "pt ", p < 0 ? 0 : p, n, mean * 255.0, r2, mae);
        for (int i = 0; i < FEATURES; i++) printf(" %+.3f", coef[i]);
        printf("\n");
    }
    /* per track, the points 2..4 and 7..9 (not the outer ones) together: where the light grows is toward the sun, whose horizontal direction is (nx, nz)'s coefficients; the
     * azimuth of that against the heading of the track's first stretch says whether the CD's sun is the world's or the track's */
    printf("per track (points 2-4, 7-9): n, R2, fitMAE | %s | light grows toward azimuth, track heading (degrees, atan2(z, x))\n", "1 sky nx nz ny");
    for (int t = 0; t < track_count; t++) {
        double coef[FEATURES], r2, mae, mean;
        long n;
        fit_lo = tracks[t].first, fit_hi = tracks[t].end;
        (void)terrain_only_inner;
        if (!fit(-2, coef, &r2, &mae, &mean, &n)) continue;
        printf("%-4s n %5ld R2 %.2f MAE %5.1f |", tracks[t].name, n, r2, mae);
        for (int i = 0; i < FEATURES; i++) printf(" %+.3f", coef[i]);
        printf(" | az %4.0f heading %4.0f\n", atan2(coef[3], coef[2]) * 180.0 / 3.14159265, tracks[t].heading);
    }
    free(samples);
    return 0;
}
