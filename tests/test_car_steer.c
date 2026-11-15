/* dep59 tools B2, Task 3: a front wheel's steering (formats/car_steer.h). A synthetic wheel turned 30 degrees is recovered; on the
 * CD the Viper's wheels fit 26.7 and 29.0 degrees about their outer side, every turn part of the 39 CFMs fits within 1.9 units,
 * and offsets generated from the Viper's fitted angles are the CD's to within 2 units. The CD's tests are skipped without it. */
#include "formats/car_steer.h"
#include "formats/cfm_build.h"
#include "formats/chunk.h"
#include "test.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static uint8_t *read_cd(const char *file, size_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/original/SIMDATA/CARFAMS/%s", TNFS_SOURCE_DIR, file);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = n > 0 ? malloc((size_t)n) : NULL;
    if (d != NULL && fread(d, 1, (size_t)n, f) != (size_t)n) free(d), d = NULL;
    fclose(f);
    *size = (size_t)n;
    return d;
}

static int near(double a, double b, double margin) { return fabs(a - b) <= margin; }

/* a wheel of 8 vertices (a box 40 wide in x, 90 long in z) on the left, its offsets from car_steer_offsets, fitted back */
static void test_synthetic(void)
{
    MeshVertex v[16];
    for (int k = 0; k < 8; k++) v[k] = (MeshVertex){ k & 1 ? -80 : -120, k & 2 ? 80 : 0, k & 4 ? 225 : 135 };
    ExchangeError err = { { 0 } };
    CHECK(car_steer_offsets(v, 8, 30.0, -1, v + 8, &err));
    for (int k = 0; k < 8; k++) CHECK_EQ_INT(v[8 + k].y, 0);
    MeshDescPart part = { "left_tu", (uint32_t[]){ 0, 1, 2, 3, 4, 5, 6, 7 }, 8, 8 };
    MeshDesc d;
    memset(&d, 0, sizeof d);
    d.vertices = v, d.vertex_count = 16, d.parts = &part, d.part_count = 1;
    CarSteer s;
    CHECK(car_steer_fit(&d, 0, &s, &err));
    CHECK(near(s.degrees, 30.0, 0.5) && near(s.pivot_x, -120, 1.5) && near(s.pivot_z, 180, 1.5) && s.residual < 1.0);
    if (!near(s.degrees, 30.0, 0.5)) fprintf(stderr, "car_steer: synthetic: %.2f degrees about (%.1f, %.1f)\n", s.degrees, s.pivot_x, s.pivot_z);
    CHECK(!car_steer_offsets(v, 0, 30.0, -1, v + 8, &err));
    CHECK(!car_steer_offsets(v, 8, 30.0, 0, v + 8, &err));
    CHECK(!car_steer_fit(&d, 1, &s, &err));
}

/* every turn part of `cfm`'s two meshes fits; their count in `parts`, the worst residual in `worst` */
static void fit_all(const char *file, const uint8_t *cfm, size_t size, int *parts, double *worst)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN];
    ExchangeError err = { { 0 } };
    if (!cfm_describe(cfm, size, child, len, &err)) { CHECK(0); return; }
    for (int level = 0; level < 2; level++) {
        Chunk c;
        Mesh m;
        MeshDesc d;
        if (chunk_open(&c, child[2 * level], len[2 * level]) != CHUNK_OK || mesh_open(&m, &c) != MESH_OK) continue;
        if (!mesh_describe(&m, &d, &err)) continue;   /* LDIABLO's low mesh: car-model.md, Exceptions */
        for (uint32_t p = 0; p < d.part_count; p++) {
            CarSteer s;
            CHECK(car_steer_fit(&d, p, &s, &err));
            (*parts)++;
            if (s.residual > *worst) *worst = s.residual;
            if (s.residual > 1.9) fprintf(stderr, "car_steer: %s part %u: residual %.2f\n", file, p, s.residual);
        }
        mesh_desc_free(&d);
    }
}

static void test_cd_parts(void)
{
    char dir[512];
    snprintf(dir, sizeof dir, "%s/original/SIMDATA/CARFAMS", TNFS_SOURCE_DIR);
    DIR *h = opendir(dir);
    if (h == NULL) return;
    int parts = 0;
    double worst = 0;
    struct dirent *de;
    while ((de = readdir(h)) != NULL) {
        const size_t n = strlen(de->d_name);
        if (n < 5 || strcasecmp(de->d_name + n - 4, ".CFM") != 0) continue;
        size_t size = 0;
        uint8_t *cfm = read_cd(de->d_name, &size);
        if (cfm != NULL) fit_all(de->d_name, cfm, size, &parts, &worst);
        free(cfm);
    }
    closedir(h);
    fprintf(stderr, "car_steer: %d turn parts, the worst residual %.2f\n", parts, worst);
    CHECK_EQ_INT(parts, 30);
    CHECK(worst <= 1.9);
}

/* the Viper: the fitted angles and pivots; offsets generated from the fitted angle against the CD's */
static void test_viper(const uint8_t *cfm, size_t size)
{
    Chunk root, c;
    Mesh m;
    MeshDesc d;
    ExchangeError err = { { 0 } };
    CHECK(chunk_open(&root, cfm, size) == CHUNK_OK && chunk_child(&root, CFM_CHILD_MESH_HIGH, &c) == CHUNK_OK);
    if (mesh_open(&m, &c) != MESH_OK || !mesh_describe(&m, &d, &err)) { CHECK(0); return; }
    CHECK_EQ_INT((int)d.part_count, 2);
    static const double WANT[2][2] = { { 26.7, -121 }, { 29.0, 123 } };
    for (uint32_t p = 0; p < d.part_count && p < 2; p++) {
        CarSteer s;
        CHECK(car_steer_fit(&d, p, &s, &err));
        CHECK(near(s.degrees, WANT[p][0], 0.2) && near(s.pivot_x, WANT[p][1], 2) && near(s.pivot_z, 180, 2));
        fprintf(stderr, "car_steer: DVIPER part %u: %.2f degrees about (%.1f, %.1f), residual %.2f\n", p, s.degrees, s.pivot_x, s.pivot_z, s.residual);
        const MeshDescPart *part = &d.parts[p];
        MeshVertex wheel[8], got[8];
        for (uint32_t k = 0; k < part->count && k < 8; k++) wheel[k] = d.vertices[part->vertices[k]];
        CHECK(car_steer_offsets(wheel, part->count, s.degrees, p == 0 ? -1 : 1, got, &err));
        int off = 0;
        for (uint32_t k = 0; k < part->count && k < 8; k++) {
            const MeshVertex want = d.vertices[part->first_offset_vertex + k];
            const int dx = abs(got[k].x - want.x), dz = abs(got[k].z - want.z);
            off = dx > off ? dx : dz > off ? dz : off;
        }
        if (off > 2) fprintf(stderr, "car_steer: DVIPER part %u: generated offsets %d units from the CD's\n", p, off);
        CHECK(off <= 2);
    }
    mesh_desc_free(&d);
}

int main(void)
{
    test_synthetic();
    size_t n = 0;
    uint8_t *viper = read_cd("DVIPER.CFM", &n);
    if (viper == NULL) {
        fprintf(stderr, "car_steer: no CD in original/, the CD's tests are skipped\n");
        return TEST_REPORT();
    }
    test_viper(viper, n);
    test_cd_parts();
    free(viper);
    return TEST_REPORT();
}
