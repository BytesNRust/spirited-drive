/* psx_light_mae <psx_dir> <data_dir> <track> [A|B|C]: how close our own light (game/race/psx_light_make.h) is to the PS1 CD's for a track, numbers only. Reads
 * <data_dir>/SIMDATA/MISC/<TRACK>.TRI (the PC game's) and <psx_dir>/Z<TRACK><letter>.LGT; writes nothing and prints no byte of the CD file, only statistics.
 * The comparison is on the terrain points (2..5 and 7..10) of the nodes that are not a tunnel's (a tunnel's rows are the synth's): the mean absolute error in
 * the 0..255 bytes, the signed bias, the share within one of the 16 levels (17), per point; and the same for each sun azimuth in 30 degree steps, the best
 * named, and for a flat 255 (what the light is without any). The time: A dawn, B day, C dusk. */
#include "game/race/psx_light.h"
#include "game/race/psx_light_make.h"

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
    double abs_sum, signed_sum;
    long within, n;
} Stat;

static void add(Stat *s, int ours, int theirs)
{
    const int d = ours - theirs;
    s->abs_sum += abs(d), s->signed_sum += d, s->within += abs(d) <= 17, s->n++;
}

static int terrain(int p) { return p != 0 && p != 1 && p != 6; }

int main(int argc, char **argv)
{
    if (argc < 4) return fprintf(stderr, "usage: psx_light_mae <psx_dir> <data_dir> <track> [A|B|C]\n"), 2;
    const char letter = argc > 4 ? (char)toupper((unsigned char)argv[4][0]) : 'B';
    const int time = letter == 'A' ? PSX_SYNTH_DAWN : letter == 'C' ? PSX_SYNTH_DUSK : PSX_SYNTH_DAY;
    char track[16], path[512];
    size_t i = 0;
    for (; argv[3][i] != '\0' && i < sizeof track - 1; i++) track[i] = (char)toupper((unsigned char)argv[3][i]);
    track[i] = '\0';
    size_t tri_size, lgt_size;
    snprintf(path, sizeof path, "%s/SIMDATA/MISC/%s.TRI", argv[2], track);
    uint8_t *tri_data = slurp(path, &tri_size);
    if (tri_data == NULL) return fprintf(stderr, "cannot read %s\n", path), 1;
    static Tri tri;
    if (tri_open(&tri, tri_data, tri_size) != TRI_OK) return fprintf(stderr, "%s is not a TRI\n", path), 1;
    snprintf(path, sizeof path, "%s/Z%s%c.LGT", argv[1], track, letter);
    uint8_t *lgt_data = slurp(path, &lgt_size);
    if (lgt_data == NULL) return fprintf(stderr, "cannot read %s\n", path), 1;
    PsxLight ps1;
    if (!psx_light_parse(lgt_data, lgt_size, tri.node_count, &ps1))
        return fprintf(stderr, "%s %c: the CD's light does not have the TRI's %u nodes\n", track, letter, tri.node_count), 1;
    const uint32_t count = tri.node_count;
    LightPos *pts = malloc((size_t)count * PSX_LIGHT_POINTS * sizeof *pts);
    uint8_t *ours = malloc((size_t)count * PSX_LIGHT_POINTS);
    if (pts == NULL || ours == NULL || !psx_light_tri_points(&tri, pts)) return fprintf(stderr, "cannot read the terrain\n"), 1;
    const int ring = tri_is_circuit(&tri);
    PsxLightSun sun;
    psx_light_sun_default(time, &sun);
    printf("%s %c: %u nodes, %s, sun elevation %.0f degrees\n", track, letter, count, ring ? "a circuit" : "a stage", sun.elevation * 180.0 / 3.14159265);
    double best = 1e9;
    int best_az = 0;
    for (int az = 0; az < 1; az += 30) {   /* the CD's sun is fixed along x (psx_light_make.c): one azimuth is all there is */
        sun.azimuth = (float)az * 3.14159265f / 180.0f;
        if (!psx_light_make_rows_sun(pts, count, ring, time, &sun, ours)) return fprintf(stderr, "no rows\n"), 1;
        Stat all = { 0 }, per[PSX_LIGHT_POINTS] = { { 0 } };
        for (uint32_t n = 0; n + 1 < count; n++) {
            if (psx_light_tunnel(&ps1, n)) continue;
            for (int p = 0; p < PSX_LIGHT_POINTS; p++) {
                if (!terrain(p)) continue;
                const int theirs = psx_light_byte(&ps1, n, p), mine = ours[n * PSX_LIGHT_POINTS + (uint32_t)p];
                add(&all, mine, theirs), add(&per[p], mine, theirs);
            }
        }
        const double mae = all.n > 0 ? all.abs_sum / (double)all.n : 0;
        printf("  azimuth %3d: MAE %5.1f, bias %+6.1f, within one level %4.1f%% (%ld points)\n", az, mae, all.n > 0 ? all.signed_sum / (double)all.n : 0,
               all.n > 0 ? 100.0 * (double)all.within / (double)all.n : 0, all.n);
        if (mae < best) best = mae, best_az = az;
        if (az == 0) {   /* the per-point split once, at the first azimuth */
            printf("    by point (MAE/bias):");
            for (int p = 0; p < PSX_LIGHT_POINTS; p++)
                if (terrain(p) && per[p].n > 0) printf(" %d:%.0f/%+.0f", p, per[p].abs_sum / (double)per[p].n, per[p].signed_sum / (double)per[p].n);
            printf("\n");
        }
    }
    Stat flat = { 0 };
    for (uint32_t n = 0; n + 1 < count; n++) {
        if (psx_light_tunnel(&ps1, n)) continue;
        for (int p = 0; p < PSX_LIGHT_POINTS; p++)
            if (terrain(p)) add(&flat, 255, psx_light_byte(&ps1, n, p));
    }
    printf("  best azimuth %d (MAE %.1f); a flat 255 would be MAE %.1f\n", best_az, best, flat.n > 0 ? flat.abs_sum / (double)flat.n : 0);
    psx_light_free(&ps1);
    free(pts), free(ours), free(tri_data), free(lgt_data);
    return 0;
}
