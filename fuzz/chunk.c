/* The chunk container of the car (CFM) and track art (FAM) files: wwww headers, ORIP meshes, SHPI bitmaps */
#include "fuzz/fuzz.h"
#include "formats/chunk.h"
#include "formats/fsh.h"
#include "formats/mesh.h"

enum { DEPTH = 8, CHILDREN = 4096, ITEMS = 4096 };

static void mesh_walk(const Chunk *c)
{
    Mesh m;
    if (mesh_open(&m, c) != MESH_OK) return;
    for (uint32_t i = 0; i < m.vertex_count && i < ITEMS; i++) { MeshVertex v; (void)mesh_vertex(&m, i, &v); }
    for (uint32_t i = 0; i < m.uv_count && i < ITEMS; i++) { MeshUv v; (void)mesh_uv(&m, i, &v); }
    for (uint32_t i = 0; i < m.polygon_count && i < ITEMS; i++) {
        MeshPolygon p;
        if (mesh_polygon(&m, i, &p) != MESH_OK) continue;
        for (uint32_t k = 0; k < 4; k++) { uint32_t slot; (void)mesh_vertex_map(&m, p.vertex_slot + k, &slot); (void)mesh_vertex_map(&m, p.uv_slot + k, &slot); }
    }
    for (uint32_t i = 0; i < m.texture_count && i < ITEMS; i++) { MeshTexture t; (void)mesh_texture(&m, i, &t); }
    for (uint32_t i = 0; i < m.section_a_count && i < ITEMS; i++) { MeshSectionA s; (void)mesh_section_a(&m, i, &s); }
    for (uint32_t i = 0; i < m.section_b_count && i < ITEMS; i++) { MeshSectionB s; (void)mesh_section_b(&m, i, &s); }
    for (uint32_t i = 0; i < m.section_c_count && i < ITEMS; i++) { MeshSectionC s; (void)mesh_section_c(&m, i, &s); }
    for (uint32_t i = 0; i < m.label_count && i < ITEMS; i++) { MeshLabel l; (void)mesh_label(&m, i, &l); }
}

static void bitmap_walk(const Chunk *c)
{
    Fsh fsh;
    if (chunk_bitmaps(c, &fsh) != FSH_OK) return;
    for (uint32_t i = 0; i < fsh.count && i < ITEMS; i++) {
        FshEntry e;
        if (fsh_entry(&fsh, i, &e) == FSH_OK) fuzz_touch(e.payload, e.payload_size);
    }
}

static void walk(const Chunk *c, int depth)
{
    if (c->kind == CHUNK_KIND_MESH) mesh_walk(c);
    else if (c->kind == CHUNK_KIND_BITMAP) bitmap_walk(c);
    if (c->kind != CHUNK_KIND_HEADER || depth >= DEPTH) return;
    for (uint32_t i = 0; i < c->count && i < CHILDREN; i++) {
        Chunk child;
        if (chunk_child(c, i, &child) == CHUNK_OK) walk(&child, depth + 1);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Chunk root;
    if (chunk_open(&root, data, size) != CHUNK_OK) return 0;
    ChunkTotals totals;
    (void)chunk_survey(&root, &totals);
    walk(&root, 0);
    return 0;
}
