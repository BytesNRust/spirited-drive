/* Integration test over the real CD data: every FAM and CFM given on the
 * command line must parse as a chunk tree, every mesh chunk must open with all
 * of its records reachable and its indexes in range, and every bitmap chunk
 * must open as an SHPI directory. Counts are reported for each file whose name
 * contains TR1_T01 (a track family) or DVIPER (a car).
 * Registered by CMake only when original/ holds the extracted CD. */
#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/mesh.h"
#include "formats/qfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t meshes, vertices, uvs, polygons, textures, labels, bitmaps, entries;
    uint32_t sections_a, sections_b, sections_c;
} Counts;

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *data = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        long len = ftell(f);
        if (len >= 0 && fseek(f, 0, SEEK_SET) == 0 && (data = malloc((size_t)len + 1)) != NULL) {
            *size = fread(data, 1, (size_t)len, f);
            if (*size != (size_t)len) {
                free(data);
                data = NULL;
            }
        }
    }
    fclose(f);
    return data;
}

static int fail(const char *path, const char *what, uint32_t index)
{
    fprintf(stderr, "%s: %s (%u)\n", path, what, index);
    return 0;
}

static int check_mesh(const char *path, const Chunk *chunk, Counts *counts)
{
    Mesh m;
    MeshResult r = mesh_open(&m, chunk);
    if (r != MESH_OK) return fail(path, mesh_result_str(r), 0);

    for (uint32_t i = 0; i < m.vertex_count; i++) {
        MeshVertex v;
        if (mesh_vertex(&m, i, &v) != MESH_OK) return fail(path, "vertex", i);
    }
    for (uint32_t i = 0; i < m.uv_count; i++) {
        MeshUv uv;
        if (mesh_uv(&m, i, &uv) != MESH_OK) return fail(path, "uv", i);
    }
    for (uint32_t i = 0; i < m.polygon_count; i++) {
        MeshPolygon p;
        uint32_t vertices[4], uvs[4];
        if (mesh_polygon(&m, i, &p) != MESH_OK) return fail(path, "polygon", i);
        const uint32_t corners = p.flags & 7u;
        /* 3 or 4 corners; a single 2-corner record exists, in the "_MINE" mesh of TR6 */
        if (corners < 2 || corners > 4) return fail(path, "polygon corner count", i);
        if (p.texture >= m.texture_count) return fail(path, "polygon texture index", i);
        if (mesh_polygon_corners(&m, &p, vertices, uvs) != MESH_OK) return fail(path, "polygon slots", i);
        for (uint32_t c = 0; c < corners; c++) {
            if (vertices[c] >= m.vertex_count) return fail(path, "polygon vertex index", i);
            if (p.uv_slot != p.vertex_slot && uvs[c] >= m.uv_count) return fail(path, "polygon uv index", i);
        }
    }
    for (uint32_t i = 0; i < m.texture_count; i++) {
        MeshTexture t;
        if (mesh_texture(&m, i, &t) != MESH_OK) return fail(path, "texture", i);
    }
    for (uint32_t i = 0; i < m.label_count; i++) {
        MeshLabel l;
        if (mesh_label(&m, i, &l) != MESH_OK || l.polygon >= m.polygon_count) return fail(path, "label", i);
    }
    for (uint32_t i = 0; i < m.section_a_count; i++) {
        MeshSectionA s;
        if (mesh_section_a(&m, i, &s) != MESH_OK) return fail(path, "section A", i);
    }
    for (uint32_t i = 0; i < m.section_b_count; i++) {
        MeshSectionB s;
        if (mesh_section_b(&m, i, &s) != MESH_OK) return fail(path, "section B", i);
    }
    for (uint32_t i = 0; i < m.section_c_count; i++) {
        MeshSectionC s;
        if (mesh_section_c(&m, i, &s) != MESH_OK) return fail(path, "section C", i);
    }
    counts->meshes++;
    counts->vertices += m.vertex_count;
    counts->uvs += m.uv_count;
    counts->polygons += m.polygon_count;
    counts->textures += m.texture_count;
    counts->labels += m.label_count;
    counts->sections_a += m.section_a_count;
    counts->sections_b += m.section_b_count;
    counts->sections_c += m.section_c_count;
    return 1;
}

static int check_bitmaps(const char *path, const Chunk *chunk, Counts *counts)
{
    Fsh fsh;
    FshResult r = chunk_bitmaps(chunk, &fsh);
    if (r != FSH_OK) return fail(path, fsh_result_str(r), 0);
    for (uint32_t i = 0; i < fsh.count; i++) {
        FshEntry e;
        r = fsh_entry(&fsh, i, &e);
        if (r != FSH_OK) return fail(path, fsh_result_str(r), i);
        counts->entries++;
    }
    counts->bitmaps++;
    return 1;
}

static int check_tree(const char *path, const Chunk *node, Counts *counts)
{
    switch (node->kind) {
    case CHUNK_KIND_ABSENT: return 1;
    case CHUNK_KIND_MESH: return check_mesh(path, node, counts);
    case CHUNK_KIND_BITMAP: return check_bitmaps(path, node, counts);
    case CHUNK_KIND_HEADER: break;
    }
    for (uint32_t i = 0; i < node->count; i++) {
        Chunk child;
        ChunkResult r = chunk_child(node, i, &child);
        if (r != CHUNK_OK) return fail(path, chunk_result_str(r), i);
        if (!check_tree(path, &child, counts)) return 0;
    }
    return 1;
}

static int check_file(const char *path, Counts *total)
{
    size_t size = 0;
    uint8_t *data = read_file(path, &size);
    if (data == NULL) return fail(path, "cannot read", 0);

    uint8_t *plain = NULL;
    if (qfs_is_compressed(data, size)) {
        size_t out_size = 0;
        QfsResult q = qfs_decompressed_size(data, size, &out_size);
        plain = q == QFS_OK ? malloc(out_size ? out_size : 1) : NULL;
        if (plain == NULL || qfs_decompress(data, size, plain, out_size) != QFS_OK) {
            free(plain);
            free(data);
            return fail(path, "QFS decompression failed", 0);
        }
        free(data);
        data = plain;
        size = out_size;
    }

    Counts counts = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    Chunk root;
    ChunkTotals totals;
    ChunkResult r = chunk_open(&root, data, size);
    if (r == CHUNK_OK) r = chunk_survey(&root, &totals);
    int ok = r == CHUNK_OK ? check_tree(path, &root, &counts) : fail(path, chunk_result_str(r), 0);
    if (ok && (totals.meshes != counts.meshes || totals.bitmaps != counts.bitmaps)) {
        ok = fail(path, "survey and tree walk disagree", 0);
    }
    if (ok && (strstr(path, "TR1_T01") != NULL || strstr(path, "DVIPER") != NULL)) {
        fprintf(stderr, "%s: %u headers, %u meshes, %u vertices, %u uvs, %u polygons, %u texture refs, "
                "%u labels, %u bitmap chunks (%u entries), depth %u, %u absent\n", path, totals.headers,
                counts.meshes, counts.vertices, counts.uvs, counts.polygons, counts.textures,
                counts.labels, counts.bitmaps, counts.entries, totals.max_depth, totals.absent);
    }
    if (ok) {
        total->meshes += counts.meshes;
        total->vertices += counts.vertices;
        total->polygons += counts.polygons;
        total->bitmaps += counts.bitmaps;
        total->entries += counts.entries;
    }
    free(data);
    return ok;
}

int main(int argc, char **argv)
{
    int failures = 0;
    Counts total = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    for (int i = 1; i < argc; i++) {
        if (!check_file(argv[i], &total)) failures++;
    }
    fprintf(stderr, "%d files, %u meshes, %u vertices, %u polygons, %u bitmap chunks (%u entries), "
            "%d failures\n", argc - 1, total.meshes, total.vertices, total.polygons, total.bitmaps,
            total.entries, failures);
    return failures == 0 && argc > 1 ? 0 : 1;
}
