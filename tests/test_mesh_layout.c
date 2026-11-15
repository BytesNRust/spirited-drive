/* dep59 tools B1: the layout of a car model (docs/modding/car-model.md), measured on every CFM of the CD and kept here as checks, so
 * that the builder (mesh_build, shpi_build, cfm_build) writes what the CD holds. Skipped without the CD, as test_models.
 *
 * A CFM: a "wwww" table of 4 children (high mesh, its bank, low mesh, its bank), each starting on 4 bytes, padded to 4 with bytes
 * of no meaning. A mesh: the 0x70 header, then polygons, uvs, texture references, sections A, B and C, labels, vertices and the
 * vertex map, each table right after the one before. The map: per polygon in order its uv run then its vertex run (as many slots as
 * corners; a polygon without uvs has the vertex run only), then each section A's vertex list, then each section B's polygon list,
 * up to the chunk's end. The one exception is LDIABLO.CFM's low mesh (19-byte texture records), a file the game never opens. */
#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/mesh.h"
#include "test.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum { HEADER = 0x70, CHILDREN = 4, MAX_FILES = 64 };

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

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

static int is_irregular(const char *file, int child) { return strcasecmp(file, "LDIABLO.CFM") == 0 && child == CFM_CHILD_MESH_LOW; }

/* the counters the summary prints: what the document states, with how many meshes hold it */
static int meshes, contiguous, header_ok, map_ok, real_ok, parts_ok, groups_ok, points_ok, low_plain, corners_ok;

/* the tables in the order the CD lays them, each right after the previous one */
static int tables_contiguous(const Mesh *m)
{
    const struct { uint32_t offset, bytes; } t[] = {
        { m->polygon_offset, m->polygon_count * 12 }, { m->uv_offset, m->uv_count * 8 },
        { m->texture_offset, m->texture_count * 20 }, { m->section_a_offset, m->section_a_count * 20 },
        { m->section_b_offset, m->section_b_count * 28 }, { m->section_c_offset, m->section_c_count * 12 },
        { m->label_offset, m->label_count * 12 }, { m->vertex_offset, m->vertex_count * 12 }, { m->vertex_map_offset, 0 },
    };
    uint32_t at = HEADER;
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        if (t[i].offset != at) return 0;
        at += t[i].bytes;
    }
    return 1;
}

/* the map: polygon runs (uv then vertex, a slot a corner), then section A lists, then section B lists, to the chunk's end */
static int map_in_order(const Mesh *m)
{
    uint32_t next = 0;
    for (uint32_t i = 0; i < m->polygon_count; i++) {
        MeshPolygon p;
        if (mesh_polygon(m, i, &p) != MESH_OK) return 0;
        const uint32_t n = p.flags & 7;
        if (n != 3 && n != 4) return 0;
        if (p.uv_slot != p.vertex_slot) {
            if (p.uv_slot != next) return 0;
            next += n;
        }
        if (p.vertex_slot != next) return 0;
        next += n;
    }
    for (uint32_t i = 0; i < m->section_a_count; i++) {
        MeshSectionA a;
        if (mesh_section_a(m, i, &a) != MESH_OK || a.unknown[1] != next) return 0;
        next += a.unknown[0];
    }
    for (uint32_t i = 0; i < m->section_b_count; i++) {
        MeshSectionB b;
        if (mesh_section_b(m, i, &b) != MESH_OK || b.unknown[1] != next) return 0;
        next += b.unknown[2];
    }
    return next == m->vertex_map_slots && m->vertex_map_offset + 4 * next == m->size;
}

/* section A: its words are (count of vertices, map slot of its list, first displaced vertex); the displaced vertices are the
 * last ones, after +0x14 */
static int parts_layout(const Mesh *m)
{
    uint32_t first = m->unknown_14;
    for (uint32_t i = 0; i < m->section_a_count; i++) {
        MeshSectionA a;
        if (mesh_section_a(m, i, &a) != MESH_OK || a.unknown[2] != first) return 0;
        first += a.unknown[0];
    }
    return first == m->vertex_count;
}

static int groups_words(const Mesh *m)
{
    for (uint32_t i = 0; i < m->section_b_count; i++) {
        MeshSectionB b;
        if (mesh_section_b(m, i, &b) != MESH_OK) return 0;
        if (!(b.unknown[0] == 0 || b.unknown[0] == 1 || b.unknown[0] == 0xffffffffu) || b.unknown[3] != 0) return 0;
    }
    return 1;
}

static int points_are_vertices(const Mesh *m)
{
    for (uint32_t i = 0; i < m->section_c_count; i++) {
        MeshSectionC c;
        if (mesh_section_c(m, i, &c) != MESH_OK || c.value >= m->vertex_count) return 0;
    }
    return 1;
}

static int corners_and_textures(const Mesh *m)
{
    for (uint32_t i = 0; i < m->polygon_count; i++) {
        MeshPolygon p;
        if (mesh_polygon(m, i, &p) != MESH_OK || ((p.flags & 7) != 3 && (p.flags & 7) != 4) || p.texture >= m->texture_count) return 0;
    }
    return 1;
}

static void check_mesh(const char *file, int child, const Chunk *c)
{
    Mesh m;
    CHECK_EQ_INT(mesh_open(&m, c), MESH_OK);
    meshes++;
#define RULE(counter, cond)                                                                     \
    do {                                                                                        \
        const int ok_ = (cond);                                                                 \
        counter += ok_;                                                                         \
        if (!ok_) fprintf(stderr, "mesh_layout: %s child %d breaks: %s\n", file, child, #cond); \
        CHECK(ok_);                                                                             \
    } while (0)
    RULE(header_ok, m.unknown_08 == 0x2bc && le32(m.base + 0x0c) == 0 && m.unknown_64 == 0 && le32(m.base + 0x6c) == 0 &&
                        m.unknown_68_offset == m.polygon_offset);
    RULE(contiguous, tables_contiguous(&m));
    RULE(map_ok, map_in_order(&m));
    RULE(parts_ok, parts_layout(&m));
    RULE(groups_ok, groups_words(&m));
    RULE(points_ok, points_are_vertices(&m));
    RULE(corners_ok, corners_and_textures(&m));
    if (child == CFM_CHILD_MESH_LOW) RULE(low_plain, m.section_a_count == 0 && m.section_c_count == 0 && m.label_count == 0);
    real_ok += m.unknown_14 <= m.vertex_count;
#undef RULE
}

/* the bank: directory id WRAP, its palettes 256 colours with misc (256, 0, 0, 0), its bitmaps 0x7b */
static int banks, banks_ok;
static void check_bank(const char *file, int child, const Chunk *c)
{
    Fsh f;
    CHECK_EQ_INT(fsh_open(&f, c->data, c->size), FSH_OK);
    int ok = memcmp(c->data + 12, "WRAP", 4) == 0;
    for (uint32_t i = 0; i < f.count; i++) {
        FshEntry e;
        if (fsh_entry(&f, i, &e) != FSH_OK) { ok = 0; continue; }
        if (e.type == FSH_TYPE_PALETTE_6BIT || e.type == FSH_TYPE_PALETTE_8BIT)
            ok &= e.width == 256 && e.misc[0] == 256 && e.misc[1] == 0 && e.misc[2] == 0 && e.misc[3] == 0;
        else
            ok &= e.type == FSH_TYPE_BITMAP;
    }
    uint32_t index;
    ok &= fsh_find(&f, "!PAL", &index) == FSH_OK;
    if (!ok) fprintf(stderr, "mesh_layout: %s child %d: a bank rule does not hold\n", file, child);
    CHECK(ok);
    banks++, banks_ok += ok;
}

static void check_file(const char *file, const uint8_t *d, size_t n)
{
    Chunk root;
    CHECK_EQ_INT(chunk_open(&root, d, n), CHUNK_OK);
    CHECK_EQ_INT((int)root.count, CHILDREN);
    CHECK_EQ_INT((int)le32(d + 8), 8 + 4 * CHILDREN);   /* the first child right after the table */
    uint32_t previous = 0;
    for (int k = 0; k < CHILDREN; k++) {
        Chunk c;
        CHECK_EQ_INT(chunk_child(&root, (uint32_t)k, &c), CHUNK_OK);
        const uint32_t at = le32(d + 8 + 4 * k), end = k + 1 < CHILDREN ? le32(d + 12 + 4 * k) : (uint32_t)n;
        CHECK(at > previous && at % 4 == 0);
        CHECK_EQ_INT((int)(end - at), (int)((c.size + 3) & ~3u));   /* the declared length, padded to 4 */
        previous = at;
        if (k == CFM_CHILD_MESH_HIGH || k == CFM_CHILD_MESH_LOW) {
            if (is_irregular(file, k)) continue;
            check_mesh(file, k, &c);
        } else {
            check_bank(file, k, &c);
        }
    }
}

static int by_name(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

int main(void)
{
    char dir[512];
    snprintf(dir, sizeof dir, "%s/original/SIMDATA/CARFAMS", TNFS_SOURCE_DIR);
    DIR *h = opendir(dir);
    if (h == NULL) {
        fprintf(stderr, "mesh_layout: no CD in original/, skipped\n");
        return TEST_REPORT();
    }
    char *names[MAX_FILES];
    int count = 0;
    struct dirent *e;
    while ((e = readdir(h)) != NULL && count < MAX_FILES) {
        const size_t len = strlen(e->d_name);
        if (len > 4 && strcasecmp(e->d_name + len - 4, ".CFM") == 0) names[count++] = strdup(e->d_name);
    }
    closedir(h);
    qsort(names, (size_t)count, sizeof *names, by_name);
    CHECK_EQ_INT(count, 39);
    for (int i = 0; i < count; i++) {
        char path[1024];
        size_t n = 0;
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        uint8_t *d = read_all(path, &n);
        CHECK(d != NULL);
        if (d != NULL) check_file(names[i], d, n);
        free(d);
        free(names[i]);
    }
    CHECK_EQ_INT(meshes, 77);   /* 39 x 2, less LDIABLO's low mesh */
    CHECK_EQ_INT(banks, 78);
    fprintf(stderr, "mesh_layout: %d meshes: header %d, tables in order %d, map in order %d, parts %d, groups %d, points %d, corners %d, "
                    "+0x14 <= vertices %d; low meshes without parts, points or labels %d; %d banks, %d as stated\n",
            meshes, header_ok, contiguous, map_ok, parts_ok, groups_ok, points_ok, corners_ok, real_ok, low_plain, banks, banks_ok);
    return TEST_REPORT();
}
