/* dep59 tools B1: a car's mesh described and built again (formats/mesh_build.h). Every mesh of the CD's 39 CFMs comes back byte for
 * byte (LDIABLO's low mesh aside, docs/modding/car-model.md); the Viper's description holds what it should; a quad added to it is
 * read back; what the builder refuses it refuses with the index or the name; a large mesh has no fixed tables to overflow. The
 * tests that read the CD are skipped without it. */
#include "formats/chunk.h"
#include "formats/mesh.h"
#include "formats/mesh_build.h"
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

static int open_mesh(const uint8_t *cfm, size_t n, int child, Mesh *m)
{
    Chunk root, c;
    return chunk_open(&root, cfm, n) == CHUNK_OK && chunk_child(&root, (uint32_t)child, &c) == CHUNK_OK && mesh_open(m, &c) == MESH_OK;
}

/* the mesh built from a description of `m`: 1 when it is m's bytes */
static int round_trip(const char *file, int child, const Mesh *m)
{
    MeshDesc d;
    ExchangeError err = { { 0 } };
    uint8_t *built = NULL;
    size_t len = 0;
    if (!mesh_describe(m, &d, &err)) {
        fprintf(stderr, "mesh_build: %s child %d: describe: %s\n", file, child, err.message);
        return 0;
    }
    int ok = mesh_build(&d, &built, &len, &err);
    if (!ok) fprintf(stderr, "mesh_build: %s child %d: build: %s\n", file, child, err.message);
    if (ok && (len != m->size || memcmp(built, m->base, len) != 0)) {
        size_t at = 0;
        while (at < len && at < m->size && built[at] == m->base[at]) at++;
        fprintf(stderr, "mesh_build: %s child %d: %zu bytes for %zu, first difference at 0x%zx\n", file, child, len, m->size, at);
        ok = 0;
    }
    free(built);
    mesh_desc_free(&d);
    return ok;
}

static int test_round_trip_cd(void)
{
    char dir[512];
    snprintf(dir, sizeof dir, "%s/original/SIMDATA/CARFAMS", TNFS_SOURCE_DIR);
    DIR *h = opendir(dir);
    if (h == NULL) return 0;
    int meshes = 0, same = 0;
    struct dirent *e;
    while ((e = readdir(h)) != NULL) {
        const size_t len = strlen(e->d_name);
        if (len < 5 || strcasecmp(e->d_name + len - 4, ".CFM") != 0) continue;
        size_t n = 0;
        uint8_t *cfm = read_cd(e->d_name, &n);
        for (int child = CFM_CHILD_MESH_HIGH; cfm != NULL && child <= CFM_CHILD_MESH_LOW; child += 2) {
            if (strcasecmp(e->d_name, "LDIABLO.CFM") == 0 && child == CFM_CHILD_MESH_LOW) continue;   /* 19-byte texture records */
            Mesh m;
            CHECK(open_mesh(cfm, n, child, &m));
            meshes++;
            same += round_trip(e->d_name, child, &m);
        }
        free(cfm);
    }
    closedir(h);
    CHECK_EQ_INT(meshes, 77);
    CHECK_EQ_INT(same, meshes);
    return 1;
}

static int has_group(const MeshDesc *d, const char *name)
{
    for (uint32_t i = 0; i < d->group_count; i++)
        if (strncmp(d->groups[i].name, name, MESH_NAME_SIZE) == 0) return 1;
    return 0;
}

static void test_describe_values(const Mesh *viper)
{
    MeshDesc d;
    ExchangeError err = { { 0 } };
    CHECK(mesh_describe(viper, &d, &err));
    CHECK_EQ_INT((int)d.vertex_count, 125);
    CHECK_EQ_INT((int)d.uv_count, 74);
    CHECK_EQ_INT((int)d.polygon_count, 81);
    CHECK_EQ_INT((int)d.texture_count, 22);
    CHECK_EQ_INT((int)d.real_vertices, 109);
    CHECK(has_group(&d, "inside") && has_group(&d, "surface") && has_group(&d, "outside"));
    CHECK_EQ_INT((int)d.part_count, 2);
    if (d.part_count == 2) {
        CHECK(memcmp(d.parts[0].name, "left_tur", 8) == 0 && memcmp(d.parts[1].name, "right_tu", 8) == 0);
        CHECK_EQ_INT((int)d.parts[0].first_offset_vertex, 109);
    }
    mesh_desc_free(&d);
}

/* the group named `name`'s index, or -1 */
static int group_index(const MeshDesc *d, const char *name)
{
    for (uint32_t i = 0; i < d->group_count; i++)
        if (strncmp(d->groups[i].name, name, MESH_NAME_SIZE) == 0) return (int)i;
    return -1;
}

/* the Viper with 4 more vertices and uvs and a quad on them, textured 0, in surface */
static int add_quad(const Mesh *viper, MeshDesc *d)
{
    ExchangeError err = { { 0 } };
    if (!mesh_describe(viper, d, &err)) return 0;
    const uint32_t v0 = d->vertex_count, u0 = d->uv_count, p0 = d->polygon_count;
    d->vertices = realloc(d->vertices, (v0 + 4) * sizeof *d->vertices);
    d->uvs = realloc(d->uvs, (u0 + 4) * sizeof *d->uvs);
    d->polygons = realloc(d->polygons, (p0 + 1) * sizeof *d->polygons);
    for (uint32_t k = 0; k < 4; k++) {
        d->vertices[v0 + k] = (MeshVertex){ (int32_t)(k & 1) * 40 - 20, 90, (int32_t)(k >> 1) * 40 - 120 };
        d->uvs[u0 + k] = (MeshUv){ (int32_t)(k & 1) * 15, (int32_t)(k >> 1) * 15 };
    }
    d->vertex_count += 4, d->uv_count += 4;
    MeshDescPolygon q = { 0 };
    q = d->polygons[0];   /* the donor's flags and bytes */
    q.flags = (uint8_t)((q.flags & ~7) | 4), q.texture = 0, q.has_uv = 1;
    for (uint32_t k = 0; k < 4; k++) q.vertex[k] = v0 + k, q.uv[k] = u0 + k;
    d->polygons[p0] = q;
    d->polygon_count++;
    const int s = group_index(d, "surface");
    if (s < 0) return 0;
    MeshDescGroup *g = &d->groups[s];
    g->polygons = realloc(g->polygons, (g->count + 1) * sizeof *g->polygons);
    g->polygons[g->count++] = p0;
    return 1;
}

static int same_desc(const MeshDesc *a, const MeshDesc *b)
{
    if (a->vertex_count != b->vertex_count || a->uv_count != b->uv_count || a->polygon_count != b->polygon_count ||
        a->group_count != b->group_count || a->part_count != b->part_count)
        return 0;
    if (memcmp(a->vertices, b->vertices, a->vertex_count * sizeof *a->vertices) != 0) return 0;
    if (memcmp(a->uvs, b->uvs, a->uv_count * sizeof *a->uvs) != 0) return 0;
    for (uint32_t i = 0; i < a->polygon_count; i++)
        if (memcmp(&a->polygons[i], &b->polygons[i], sizeof a->polygons[i]) != 0) return 0;
    for (uint32_t i = 0; i < a->group_count; i++)
        if (a->groups[i].count != b->groups[i].count || memcmp(a->groups[i].polygons, b->groups[i].polygons, a->groups[i].count * 4) != 0)
            return 0;
    return 1;
}

static void test_add_quad(const Mesh *viper)
{
    MeshDesc d, again;
    ExchangeError err = { { 0 } };
    uint8_t *built = NULL;
    size_t len = 0;
    CHECK(add_quad(viper, &d));
    CHECK(mesh_build(&d, &built, &len, &err));
    Chunk c = { CHUNK_KIND_MESH, built, len, 0 };
    Mesh m;
    CHECK_EQ_INT(built != NULL ? mesh_open(&m, &c) : MESH_ERR_KIND, MESH_OK);
    if (built != NULL) {
        CHECK_EQ_INT((int)m.vertex_count, 129);
        CHECK_EQ_INT((int)m.polygon_count, 82);
        MeshPolygon p;
        uint32_t v[4], u[4];
        CHECK(mesh_polygon(&m, 81, &p) == MESH_OK && mesh_polygon_corners(&m, &p, v, u) == MESH_OK);
        for (int k = 0; k < 4; k++) CHECK_EQ_INT((int)v[k], 125 + k);
        CHECK(mesh_describe(&m, &again, &err));
        CHECK(same_desc(&d, &again));
        mesh_desc_free(&again);
    }
    free(built);
    mesh_desc_free(&d);
}

/* mesh_build of `d` must refuse, with `word` in its message */
static void refused(const MeshDesc *d, const char *word, const char *what)
{
    ExchangeError err = { { 0 } };
    uint8_t *built = NULL;
    size_t len = 0;
    const int ok = mesh_build(d, &built, &len, &err);
    if (ok || strstr(err.message, word) == NULL) fprintf(stderr, "mesh_build: %s: %s (%s)\n", what, ok ? "built" : err.message, word);
    CHECK_EQ_INT(ok, 0);
    CHECK(strstr(err.message, word) != NULL);
    CHECK(built == NULL);
    free(built);
}

static void test_refusals(const Mesh *viper)
{
    MeshDesc d;
    ExchangeError err = { { 0 } };
    CHECK(mesh_describe(viper, &d, &err));
    const MeshDescPolygon keep = d.polygons[3];
    d.polygons[3].vertex[1] = 9999;
    refused(&d, "9999", "a corner past the vertices");
    d.polygons[3] = keep;
    d.polygons[3].flags = (uint8_t)((keep.flags & ~7) | 5);
    refused(&d, "polygon 3", "a polygon of 5 corners");
    d.polygons[3] = keep;
    d.polygons[3].texture = 200;
    refused(&d, "200", "a texture past the references");
    d.polygons[3] = keep;
    const int s = group_index(&d, "surface");
    MeshDescGroup *g = &d.groups[s];
    const uint32_t first = g->polygons[0];
    g->polygons[1] = first;
    refused(&d, "surface", "a polygon twice in a group");
    mesh_desc_free(&d);
    CHECK(mesh_describe(viper, &d, &err));
    const int in = group_index(&d, "inside");
    char name[MESH_NAME_SIZE];
    memcpy(name, d.groups[in].name, sizeof name);
    memcpy(d.groups[group_index(&d, "surface")].name, name, sizeof name);
    refused(&d, "inside", "two groups of one name");
    mesh_desc_free(&d);
    CHECK(mesh_describe(viper, &d, &err));
    const uint32_t vertices = d.vertex_count, polygons = d.polygon_count;
    d.vertex_count = 0;
    refused(&d, "vertex", "no vertex");
    d.vertex_count = vertices, d.polygon_count = 0;
    refused(&d, "polygon", "no polygon");
    d.polygon_count = polygons;
    d.parts[1].first_offset_vertex = vertices - 2;
    refused(&d, "right_tu", "a part's offsets past the vertices");
    mesh_desc_free(&d);
    char field[MESH_PART_NAME_SIZE];
    CHECK(mesh_desc_name(field, sizeof field, "left_tur", &err));
    CHECK(!mesh_desc_name(field, sizeof field, "left_turn", &err) && strstr(err.message, "left_turn") != NULL);
    char mesh_name[MESH_NAME_SIZE];
    CHECK(!mesh_desc_name(mesh_name, sizeof mesh_name, "THIRTEEN_CHAR", &err) && strstr(err.message, "THIRTEEN_CHAR") != NULL);
}

/* 2000 polygons on 4000 vertices, one group: built and read back (no fixed tables) */
static void test_large(void)
{
    MeshDesc d;
    memset(&d, 0, sizeof d);
    d.unknown_08 = 0x2bc;
    d.vertex_count = d.real_vertices = 4000, d.polygon_count = 2000, d.texture_count = 1, d.group_count = 1;
    d.vertices = calloc(d.vertex_count, sizeof *d.vertices);
    d.polygons = calloc(d.polygon_count, sizeof *d.polygons);
    d.textures = calloc(1, sizeof *d.textures);
    d.groups = calloc(1, sizeof *d.groups);
    ExchangeError err = { { 0 } };
    CHECK(mesh_desc_name(d.name, sizeof d.name, "_BIG", &err) && mesh_desc_name(d.groups[0].name, MESH_NAME_SIZE, "NON-SORT", &err));
    d.groups[0].polygons = calloc(d.polygon_count, sizeof(uint32_t));
    d.groups[0].count = d.polygon_count;
    for (uint32_t i = 0; i < d.vertex_count; i++) d.vertices[i] = (MeshVertex){ (int32_t)(i % 64), (int32_t)(i / 64), 0 };
    for (uint32_t i = 0; i < d.polygon_count; i++) {
        d.polygons[i].flags = 0x84;
        for (uint32_t k = 0; k < 4; k++) d.polygons[i].vertex[k] = (2 * i + k) % d.vertex_count;
        d.groups[0].polygons[i] = i;
    }
    uint8_t *built = NULL;
    size_t len = 0;
    CHECK(mesh_build(&d, &built, &len, &err));
    Chunk c = { CHUNK_KIND_MESH, built, len, 0 };
    Mesh m;
    CHECK_EQ_INT(built != NULL ? mesh_open(&m, &c) : MESH_ERR_KIND, MESH_OK);
    if (built != NULL) CHECK_EQ_INT((int)m.polygon_count, 2000);
    free(built);
    mesh_desc_free(&d);
}

int main(void)
{
    test_large();
    size_t n = 0;
    uint8_t *viper = read_cd("DVIPER.CFM", &n);
    Mesh m;
    if (viper == NULL || !open_mesh(viper, n, CFM_CHILD_MESH_HIGH, &m)) {
        fprintf(stderr, "mesh_build: no CD in original/, the CD's tests are skipped\n");
        free(viper);
        return TEST_REPORT();
    }
    test_round_trip_cd();
    test_describe_values(&m);
    test_add_quad(&m);
    test_refusals(&m);
    free(viper);
    return TEST_REPORT();
}
