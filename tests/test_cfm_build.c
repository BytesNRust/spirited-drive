/* dep59 tools B1: a car model file put together from its four chunks (formats/cfm_build.h). Each of the CD's 39 CFMs comes back
 * byte for byte from its children; the Viper with one quad more, built into a CFM with the Viper's banks, passes validate with no
 * finding; a child of the wrong kind or length is refused naming its place. The tests that read the CD are skipped without it. */
#include "formats/cfm_build.h"
#include "formats/chunk.h"
#include "formats/mesh.h"
#include "formats/mesh_build.h"
#include "formats/validate.h"
#include "test.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static uint8_t *read_all(const char *path, size_t *size)
{
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

static uint8_t *read_cd(const char *file, size_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/original/SIMDATA/CARFAMS/%s", TNFS_SOURCE_DIR, file);
    return read_all(path, size);
}

static int round_trip(const char *file, const uint8_t *cfm, size_t size)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN], built_len = 0;
    uint8_t *built = NULL;
    ExchangeError err = { { 0 } };
    if (!cfm_describe(cfm, size, child, len, &err)) {
        fprintf(stderr, "cfm_build: %s: describe: %s\n", file, err.message);
        return 0;
    }
    int ok = cfm_build(child, len, &built, &built_len, &err);
    if (!ok) fprintf(stderr, "cfm_build: %s: build: %s\n", file, err.message);
    if (ok && (built_len != size || memcmp(built, cfm, size) != 0)) {
        fprintf(stderr, "cfm_build: %s: %zu bytes for %zu\n", file, built_len, size);
        ok = 0;
    }
    free(built);
    return ok;
}

static void test_round_trip_cd(void)
{
    char dir[512];
    snprintf(dir, sizeof dir, "%s/original/SIMDATA/CARFAMS", TNFS_SOURCE_DIR);
    DIR *h = opendir(dir);
    CHECK(h != NULL);
    if (h == NULL) return;
    int files = 0, same = 0;
    struct dirent *de;
    while ((de = readdir(h)) != NULL) {
        const size_t len = strlen(de->d_name);
        if (len < 5 || strcasecmp(de->d_name + len - 4, ".CFM") != 0) continue;
        size_t n = 0;
        uint8_t *cfm = read_cd(de->d_name, &n);
        files++;
        same += cfm != NULL && round_trip(de->d_name, cfm, n);
        free(cfm);
    }
    closedir(h);
    CHECK_EQ_INT(files, 39);
    CHECK_EQ_INT(same, files);
}

/* the Viper's high mesh with a quad more, on 4 new vertices and uvs, textured 0, in surface */
static int quad_mesh(const uint8_t *viper, size_t size, uint8_t **out, size_t *out_len)
{
    Chunk root, c;
    Mesh m;
    MeshDesc d;
    ExchangeError err = { { 0 } };
    if (chunk_open(&root, viper, size) != CHUNK_OK || chunk_child(&root, CFM_CHILD_MESH_HIGH, &c) != CHUNK_OK) return 0;
    if (mesh_open(&m, &c) != MESH_OK || !mesh_describe(&m, &d, &err)) return 0;
    const uint32_t v0 = d.vertex_count, u0 = d.uv_count, p0 = d.polygon_count;
    d.vertices = realloc(d.vertices, (v0 + 4) * sizeof *d.vertices);
    d.uvs = realloc(d.uvs, (u0 + 4) * sizeof *d.uvs);
    d.polygons = realloc(d.polygons, (p0 + 1) * sizeof *d.polygons);
    for (uint32_t k = 0; k < 4; k++) {
        d.vertices[v0 + k] = (MeshVertex){ (int32_t)(k & 1) * 40 - 20, 90, (int32_t)(k >> 1) * 40 - 120 };
        d.uvs[u0 + k] = (MeshUv){ (int32_t)(k & 1) * 15, (int32_t)(k >> 1) * 15 };
    }
    d.vertex_count += 4, d.uv_count += 4;
    MeshDescPolygon q = d.polygons[0];
    q.flags = (uint8_t)((q.flags & ~7) | 4), q.texture = 0, q.has_uv = 1;
    for (uint32_t k = 0; k < 4; k++) q.vertex[k] = v0 + k, q.uv[k] = u0 + k;
    d.polygons[d.polygon_count++] = q;
    int ok = 0;
    for (uint32_t i = 0; i < d.group_count && !ok; i++) {
        MeshDescGroup *g = &d.groups[i];
        if (strncmp(g->name, "surface", MESH_NAME_SIZE) != 0) continue;
        g->polygons = realloc(g->polygons, (g->count + 1) * sizeof *g->polygons);
        g->polygons[g->count++] = p0;
        ok = mesh_build(&d, out, out_len, &err);
    }
    mesh_desc_free(&d);
    return ok;
}

static size_t findings(const uint8_t *cfm, size_t size, const char *name)
{
    Report r;
    report_init(&r);
    validate_file(cfm, size, name, &r);
    if (r.count > 0) report_print(&r, VALIDATE_COMPATIBLE, stderr);
    const size_t n = r.count;
    report_free(&r);
    return n;
}

static void test_quad_validates(const uint8_t *viper, size_t size)
{
    const uint8_t *child[CFM_CHILDREN];
    size_t len[CFM_CHILDREN], built_len = 0, mesh_len = 0;
    uint8_t *built = NULL, *mesh = NULL;
    ExchangeError err = { { 0 } };
    CHECK(cfm_describe(viper, size, child, len, &err));
    CHECK(quad_mesh(viper, size, &mesh, &mesh_len));
    CHECK_EQ_INT((int)findings(viper, size, "DVIPER.CFM"), 0);
    child[CFM_CHILD_MESH_HIGH] = mesh, len[CFM_CHILD_MESH_HIGH] = mesh_len;   /* no padding: the builder adds it */
    CHECK(mesh != NULL && cfm_build(child, len, &built, &built_len, &err));
    if (built != NULL) {
        CHECK_EQ_INT((int)(built_len % 4), 0);
        CHECK_EQ_INT((int)findings(built, built_len, "NEWCAR.CFM"), 0);
        Chunk root, c;
        Mesh m;
        CHECK(chunk_open(&root, built, built_len) == CHUNK_OK && chunk_child(&root, CFM_CHILD_MESH_HIGH, &c) == CHUNK_OK);
        CHECK(mesh_open(&m, &c) == MESH_OK && m.polygon_count == 82);
    }
    free(mesh);
    free(built);
}

/* cfm_build of `child` must refuse, naming `word` */
static void refused(const uint8_t *const child[CFM_CHILDREN], const size_t len[CFM_CHILDREN], const char *word, const char *what)
{
    uint8_t *built = NULL;
    size_t n = 0;
    ExchangeError err = { { 0 } };
    const int ok = cfm_build(child, len, &built, &n, &err);
    if (ok || strstr(err.message, word) == NULL) fprintf(stderr, "cfm_build: %s: %s (%s)\n", what, ok ? "built" : err.message, word);
    CHECK_EQ_INT(ok, 0);
    CHECK(strstr(err.message, word) != NULL);
    CHECK(built == NULL);
    free(built);
}

static void test_refusals(void)
{
    static const uint8_t orip[8] = { 'O', 'R', 'I', 'P', 8, 0, 0, 0 }, shpi[16] = { 'S', 'H', 'P', 'I', 16, 0, 0, 0 };
    static const uint8_t long_orip[8] = { 'O', 'R', 'I', 'P', 9, 0, 0, 0 }, short_shpi[8] = { 'S', 'H', 'P', 'I', 7, 0, 0, 0 };
    const uint8_t *child[CFM_CHILDREN] = { orip, shpi, orip, shpi };
    size_t len[CFM_CHILDREN] = { 8, 16, 8, 16 };
    uint8_t *built = NULL;
    size_t n = 0;
    ExchangeError err = { { 0 } };
    CHECK(cfm_build(child, len, &built, &n, &err));
    CHECK_EQ_INT((int)n, 24 + 8 + 16 + 8 + 16);
    free(built);
    child[1] = orip, len[1] = 8;
    refused(child, len, "child 1", "a mesh where the bank goes");
    child[1] = shpi, len[1] = 16, child[2] = shpi, len[2] = 16;
    refused(child, len, "child 2", "a bank where the low mesh goes");
    child[2] = long_orip, len[2] = 8;
    refused(child, len, "child 2", "a declared length past the bytes");
    child[2] = orip, child[3] = short_shpi, len[3] = 8;
    refused(child, len, "child 3", "a declared length under 8");
    child[3] = shpi, len[3] = 4;
    refused(child, len, "child 3", "a child of 4 bytes");
    child[3] = NULL, len[3] = 0;
    refused(child, len, "child 3", "no child");
}

int main(void)
{
    test_refusals();
    size_t n = 0;
    uint8_t *viper = read_cd("DVIPER.CFM", &n);
    if (viper == NULL) {
        fprintf(stderr, "cfm_build: no CD in original/, the CD's tests are skipped\n");
        return TEST_REPORT();
    }
    test_round_trip_cd();
    test_quad_validates(viper, n);
    free(viper);
    return TEST_REPORT();
}
