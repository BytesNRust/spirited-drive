/* sky_glow <psx_dir> [--sign S] [--offset DEG] [--turn T] [--plot DIR] [TRACK ...]: where the PS1 sky's glow is, as the azimuth the track light file's `sun AZ - -` line takes
 * (track_light_file.h: degrees in the world's axes, 0 toward +x, 90 toward +z), for each track's dawn (A), day (B) and dusk (C) sky.
 * Reads <psx_dir>/Z<TRACK><A|C>.PSH, read only; prints the numbers and writes nothing of the disc: --plot DIR writes one <TRACK><letter>.ppm a track and time, a graph of the brightness
 * of each column of the panorama (no picture of the sky) with the peak marked, to check one track by eye against the game.
 *
 * CALIBRATED ON TR1 AT DAWN ONLY, TO ABOUT 25 DEGREES. What is settled:
 *   - a node's heading (the camera's yaw is TriNode.orientation * 0x400, 0x4000 a turn) is the world azimuth  90 - 360 * heading_turns  (--heading prints the fit: 100 % of the nodes of TR1, AL1, CY1 and TR4);
 *   - the glow's yaw is its panorama column over the width, with no half-turn in it (the captures of TR1 dawn at nodes 168 and 207, Z-files from the disc, show the glow right of the screen's centre where
 *     that puts it); so azimuth = 90 - 360 * (column / width), the defaults `--sign -1 --offset 90 --turn 0`;
 *   - what is NOT settled: the exact pixel scale of the view (the glow sits at about 1000 of 1280 px in both captures while the heading moves 0.15 turn: the car's yaw is not the node's), so +-25 degrees.
 * To check a track: drive it at dawn or dusk (psx_dir set) and look where the glow is against the road's heading (90 - 360 * orientation); if it is off by a fixed angle give --offset. Only the PSH files
 * whose eight tiles are of one size make a panorama (CY1 B, CY3, TR1, TR4 of the 16 tracks); the others keep the PC's sky, where the tool prints "no sky".
 * The glow is the brightest stretch (a window of 1/16 of the width, circular) of the mean luminance of the rows above the horizon row; "weak" when its peak is less than 15 % over the mean. */
#include "formats/psx_bigf.h"
#include "formats/tri.h"
#include "game/race/psx_sky.h"
#include "game/race/psx_sky_bitmap.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct {
    int peak;      /* the peak's column in the PSH's panorama */
    double ratio;  /* the peak's brightness over the mean */
} Glow;

/* the mean luminance of each column above the horizon row, smoothed over a circular window; fills `curve` (width values) and finds the peak */
static int glow_find(const PsxSky *sky, double *curve, Glow *out)
{
    const int w = sky->width, rows = sky->height * PSX_SKY_HORIZON_NUM / PSX_SKY_HORIZON_DEN;
    if (w < 16 || rows < 1) return 0;
    double *raw = calloc((size_t)w, sizeof *raw);
    if (raw == NULL) return 0;
    double mean = 0;
    for (int x = 0; x < w; x++) {
        double sum = 0;
        for (int y = 0; y < rows; y++) {
            const uint8_t *p = &sky->rgb[((size_t)y * (size_t)w + (size_t)x) * 3];
            sum += 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
        }
        raw[x] = sum / rows;
        mean += raw[x];
    }
    mean /= w;
    const int half = w / 32;
    int best = 0;
    for (int x = 0; x < w; x++) {
        double sum = 0;
        for (int d = -half; d <= half; d++) sum += raw[(x + d + w) % w];
        curve[x] = sum / (2 * half + 1);
        if (curve[x] > curve[best]) best = x;
    }
    free(raw);
    out->peak = best;
    out->ratio = mean > 0 ? curve[best] / mean : 0;
    return 1;
}

static void plot(const char *dir, const char *track, char letter, const double *curve, int w, int peak)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s%c.ppm", dir, track, letter);
    FILE *f = fopen(path, "wb");
    if (f == NULL) return;
    const int W = 512, H = 128;
    double top = 1;
    for (int x = 0; x < w; x++) if (curve[x] > top) top = curve[x];
    uint8_t *img = calloc((size_t)W * H, 3);
    if (img != NULL) {
        for (int x = 0; x < W; x++) {
            const int c = x * w / W, v = (int)(curve[c] / top * (H - 1));
            for (int y = 0; y <= v; y++) memset(&img[((size_t)(H - 1 - y) * W + (size_t)x) * 3], 200, 3);
        }
        const int px = peak * W / w;
        for (int y = 0; y < H; y++) img[((size_t)y * W + (size_t)px) * 3] = 255, img[((size_t)y * W + (size_t)px) * 3 + 1] = 0, img[((size_t)y * W + (size_t)px) * 3 + 2] = 0;
        fprintf(f, "P6\n%d %d\n255\n", W, H);
        fwrite(img, 3, (size_t)W * H, f);
        free(img);
    }
    fclose(f);
}

static double wrap180(double a)
{
    a = fmod(a, 360.0);
    if (a > 180.0) a -= 360.0;
    if (a <= -180.0) a += 360.0;
    return a;
}

/* --heading <data_dir> TRACK: how a node's heading (TriNode.orientation, 0x4000 a turn: the camera's yaw is orientation * 0x400) lies against the world's axes (0 toward +x, 90 toward +z): for nodes
 * along the road, atan2(dz, dx) of the way to the next node against the heading; prints `az = sign * 360 * heading + offset` fitted, with how many nodes agree. */
static int heading_fit(const char *data, const char *track)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/SIMDATA/MISC/%s.TRI", data, track);
    size_t size = 0;
    uint8_t *bytes = slurp(path, &size);
    static Tri tri;
    if (bytes == NULL || tri_open(&tri, bytes, size) != TRI_OK) return fprintf(stderr, "cannot read %s\n", path), 1;
    for (int sign = 1; sign >= -1; sign -= 2) {
        int agree = 0, total = 0;
        double sx = 0, sy = 0;
        for (uint32_t n = 0; n + 1 < tri.node_count; n += 3) {
            TriNode a, b;
            if (tri_node(&tri, n, &a) != TRI_OK || tri_node(&tri, n + 1, &b) != TRI_OK) continue;
            const double dx = (double)b.position.x - a.position.x, dz = (double)b.position.z - a.position.z;
            if (dx * dx + dz * dz < 1e6) continue;
            const double az = atan2(dz, dx) * 180.0 / M_PI, heading = (double)(a.orientation & 0x3fff) / 16384.0 * 360.0;
            const double off = az - sign * heading;
            sx += cos(off * M_PI / 180.0), sy += sin(off * M_PI / 180.0), total++;
        }
        const double mean = atan2(sy, sx) * 180.0 / M_PI;
        for (uint32_t n = 0; n + 1 < tri.node_count; n += 3) {
            TriNode a, b;
            if (tri_node(&tri, n, &a) != TRI_OK || tri_node(&tri, n + 1, &b) != TRI_OK) continue;
            const double dx = (double)b.position.x - a.position.x, dz = (double)b.position.z - a.position.z;
            if (dx * dx + dz * dz < 1e6) continue;
            const double d = wrap180(atan2(dz, dx) * 180.0 / M_PI - sign * (double)(a.orientation & 0x3fff) / 16384.0 * 360.0 - mean);
            if (fabs(d) < 10) agree++;
        }
        printf("%s: sign %+d offset %.1f: %d of %d nodes within 10 degrees\n", track, sign, mean, agree, total);
    }
    free(bytes);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 4 && strcmp(argv[1], "--heading") == 0) return heading_fit(argv[2], argv[3]);
    const char *psx = NULL, *plot_dir = NULL;
    double sign = -1, offset = 90, turn = 0;
    const char *tracks[32];
    int n = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sign") == 0 && i + 1 < argc) sign = atof(argv[++i]) < 0 ? -1 : 1;
        else if (strcmp(argv[i], "--offset") == 0 && i + 1 < argc) offset = atof(argv[++i]);
        else if (strcmp(argv[i], "--turn") == 0 && i + 1 < argc) turn = atof(argv[++i]);
        else if (strcmp(argv[i], "--plot") == 0 && i + 1 < argc) plot_dir = argv[++i];
        else if (psx == NULL) psx = argv[i];
        else if (n < 32) tracks[n++] = argv[i];
    }
    if (psx == NULL) {
        fprintf(stderr, "usage: sky_glow <psx_dir> [--sign S] [--offset DEG] [--turn T] [--plot DIR] [TRACK ...]\n");
        return 2;
    }
    static const char *ALL[] = { "AL1", "AL2", "AL3", "CL1", "CL2", "CL3", "CY1", "CY2", "CY3", "TR1", "TR2", "TR3", "TR4", "TR5", "TR6", "TR7" };
    if (n == 0) for (int i = 0; i < 16; i++) tracks[n++] = ALL[i];
    int missing = 0;
    for (int t = 0; t < n; t++) {
        for (const char *l = "ABC"; *l != '\0'; l++) {
            char upper[16], path[1024];
            size_t k = 0;
            for (; tracks[t][k] != '\0' && k < sizeof upper - 1; k++) upper[k] = (char)toupper((unsigned char)tracks[t][k]);
            upper[k] = '\0';
            snprintf(path, sizeof path, "%s/Z%s%c.PSH", psx, upper, *l);
            size_t size = 0;
            uint8_t *psh = slurp(path, &size);
            PsxSky sky = { 0 };
            if (psh == NULL || !psx_sky_panorama(psh, size, &sky)) {
                printf("%s %c  no sky (%s)\n", upper, *l, path);
                missing++;
                free(psh);
                continue;
            }
            double *curve = calloc((size_t)sky.width, sizeof *curve);
            Glow g = { 0, 0 };
            if (curve != NULL && glow_find(&sky, curve, &g)) {
                const double col = fmod((double)g.peak + turn * sky.width, (double)sky.width);   /* the bitmap's column, after the turn */
                const double az = wrap180(sign * 360.0 * col / sky.width + offset);
                printf("%s %c  az %.0f  (panorama column %d of %d, peak %.2f x the mean%s)\n", upper, *l, az, g.peak, sky.width, g.ratio, g.ratio < 1.15 ? ", weak: leave the default" : "");
                if (plot_dir != NULL) plot(plot_dir, upper, *l, curve, sky.width, g.peak);
            }
            free(curve);
            psx_sky_free(&sky);
            free(psh);
        }
    }
    return missing == 2 * n ? 1 : 0;
}
