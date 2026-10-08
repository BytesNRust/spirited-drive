#include "formats/mesh.h"

#include "formats/le.h"

#include <string.h>

enum {
    HEADER_SIZE = 0x70,
    VERTEX_SIZE = 12,
    UV_SIZE = 8,
    POLYGON_SIZE = 12,   /* 00490ea8 strides the polygon table by 0xC */
    TEXTURE_SIZE = 20,
    SECTION_A_SIZE = 20,
    SECTION_B_SIZE = 28,
    SECTION_C_SIZE = 12,
    LABEL_SIZE = 12,
    SLOT_SIZE = 4,
    POLYGON_CORNERS = 4
};

/* Header field offsets, see mesh.h. */
enum {
    H_UNKNOWN_08 = 0x08,
    H_VERTEX_COUNT = 0x10,
    H_UNKNOWN_14 = 0x14,
    H_VERTEX_OFFSET = 0x18,
    H_UV_COUNT = 0x1C,
    H_UV_OFFSET = 0x20,
    H_POLYGON_COUNT = 0x24,
    H_POLYGON_OFFSET = 0x28,
    H_NAME = 0x2C,
    H_TEXTURE_COUNT = 0x38,
    H_TEXTURE_OFFSET = 0x3C,
    H_SECTION_A_COUNT = 0x40,
    H_SECTION_A_OFFSET = 0x44,
    H_SECTION_B_COUNT = 0x48,
    H_SECTION_B_OFFSET = 0x4C,
    H_VERTEX_MAP_OFFSET = 0x50,
    H_SECTION_C_COUNT = 0x54,
    H_SECTION_C_OFFSET = 0x58,
    H_LABEL_COUNT = 0x5C,
    H_LABEL_OFFSET = 0x60,
    H_UNKNOWN_64 = 0x64,
    H_UNKNOWN_68 = 0x68
};

static int table_fits(size_t size, uint32_t offset, uint32_t count, uint32_t stride)
{
    const uint64_t end = (uint64_t)offset + (uint64_t)count * stride;
    return offset >= HEADER_SIZE && end <= size;
}

static uint32_t field(const uint8_t *base, uint32_t at)
{
    return le_u32(base + at);
}

MeshResult mesh_open(Mesh *mesh, const Chunk *chunk)
{
    if (chunk->kind != CHUNK_KIND_MESH) return MESH_ERR_KIND;
    if (chunk->size < HEADER_SIZE) return MESH_ERR_BOUNDS;
    const uint8_t *b = chunk->data;

    Mesh m;
    memset(&m, 0, sizeof m);
    m.base = b;
    m.size = chunk->size;
    memcpy(m.name, b + H_NAME, sizeof m.name);
    m.unknown_08 = field(b, H_UNKNOWN_08);
    m.unknown_14 = field(b, H_UNKNOWN_14);
    m.unknown_64 = field(b, H_UNKNOWN_64);
    m.unknown_68_offset = field(b, H_UNKNOWN_68);
    m.vertex_count = field(b, H_VERTEX_COUNT);
    m.vertex_offset = field(b, H_VERTEX_OFFSET);
    m.uv_count = field(b, H_UV_COUNT);
    m.uv_offset = field(b, H_UV_OFFSET);
    m.polygon_count = field(b, H_POLYGON_COUNT);
    m.polygon_offset = field(b, H_POLYGON_OFFSET);
    m.texture_count = field(b, H_TEXTURE_COUNT);
    m.texture_offset = field(b, H_TEXTURE_OFFSET);
    m.section_a_count = field(b, H_SECTION_A_COUNT);
    m.section_a_offset = field(b, H_SECTION_A_OFFSET);
    m.section_b_count = field(b, H_SECTION_B_COUNT);
    m.section_b_offset = field(b, H_SECTION_B_OFFSET);
    m.section_c_count = field(b, H_SECTION_C_COUNT);
    m.section_c_offset = field(b, H_SECTION_C_OFFSET);
    m.label_count = field(b, H_LABEL_COUNT);
    m.label_offset = field(b, H_LABEL_OFFSET);
    m.vertex_map_offset = field(b, H_VERTEX_MAP_OFFSET);

    if (!table_fits(m.size, m.vertex_offset, m.vertex_count, VERTEX_SIZE)
        || !table_fits(m.size, m.uv_offset, m.uv_count, UV_SIZE)
        || !table_fits(m.size, m.polygon_offset, m.polygon_count, POLYGON_SIZE)
        || !table_fits(m.size, m.texture_offset, m.texture_count, TEXTURE_SIZE)
        || !table_fits(m.size, m.section_a_offset, m.section_a_count, SECTION_A_SIZE)
        || !table_fits(m.size, m.section_b_offset, m.section_b_count, SECTION_B_SIZE)
        || !table_fits(m.size, m.section_c_offset, m.section_c_count, SECTION_C_SIZE)
        || !table_fits(m.size, m.label_offset, m.label_count, LABEL_SIZE)
        || !table_fits(m.size, m.vertex_map_offset, 0, SLOT_SIZE)) {
        return MESH_ERR_BOUNDS;
    }
    m.vertex_map_slots = (uint32_t)((m.size - m.vertex_map_offset) / SLOT_SIZE);
    *mesh = m;
    return MESH_OK;
}

/* Record i of a table already validated by mesh_open. */
static const uint8_t *record(const Mesh *mesh, uint32_t offset, uint32_t count, uint32_t stride,
                             uint32_t index)
{
    if (index >= count) return NULL;
    return mesh->base + offset + (size_t)index * stride;
}

MeshResult mesh_vertex(const Mesh *mesh, uint32_t index, MeshVertex *out)
{
    const uint8_t *p = record(mesh, mesh->vertex_offset, mesh->vertex_count, VERTEX_SIZE, index);
    if (p == NULL) return MESH_ERR_INDEX;
    out->x = (int32_t)le_u32(p);
    out->y = (int32_t)le_u32(p + 4);
    out->z = (int32_t)le_u32(p + 8);
    return MESH_OK;
}

MeshResult mesh_uv(const Mesh *mesh, uint32_t index, MeshUv *out)
{
    const uint8_t *p = record(mesh, mesh->uv_offset, mesh->uv_count, UV_SIZE, index);
    if (p == NULL) return MESH_ERR_INDEX;
    out->u = (int32_t)le_u32(p);
    out->v = (int32_t)le_u32(p + 4);
    return MESH_OK;
}

MeshResult mesh_polygon(const Mesh *mesh, uint32_t index, MeshPolygon *out)
{
    const uint8_t *p = record(mesh, mesh->polygon_offset, mesh->polygon_count, POLYGON_SIZE, index);
    if (p == NULL) return MESH_ERR_INDEX;
    out->flags = p[0];
    out->unknown_1 = p[1];
    out->texture = p[2];
    out->unknown_3 = p[3];
    out->vertex_slot = le_u32(p + 4);
    out->uv_slot = le_u32(p + 8);
    return MESH_OK;
}

MeshResult mesh_texture(const Mesh *mesh, uint32_t index, MeshTexture *out)
{
    const uint8_t *p = record(mesh, mesh->texture_offset, mesh->texture_count, TEXTURE_SIZE, index);
    if (p == NULL) return MESH_ERR_INDEX;
    out->unknown_0[0] = le_u32(p);
    out->unknown_0[1] = le_u32(p + 4);
    memcpy(out->name, p + 8, sizeof out->name);
    out->unknown_1[0] = le_u32(p + 12);
    out->unknown_1[1] = le_u32(p + 16);
    return MESH_OK;
}

MeshResult mesh_section_a(const Mesh *mesh, uint32_t index, MeshSectionA *out)
{
    const uint8_t *p = record(mesh, mesh->section_a_offset, mesh->section_a_count, SECTION_A_SIZE, index);
    if (p == NULL) return MESH_ERR_INDEX;
    memcpy(out->name, p, sizeof out->name);
    for (int i = 0; i < 3; i++) out->unknown[i] = le_u32(p + 8 + 4 * i);
    return MESH_OK;
}

MeshResult mesh_section_b(const Mesh *mesh, uint32_t index, MeshSectionB *out)
{
    const uint8_t *p = record(mesh, mesh->section_b_offset, mesh->section_b_count, SECTION_B_SIZE, index);
    if (p == NULL) return MESH_ERR_INDEX;
    memcpy(out->name, p, sizeof out->name);
    for (int i = 0; i < 4; i++) out->unknown[i] = le_u32(p + 12 + 4 * i);
    return MESH_OK;
}

MeshResult mesh_section_c(const Mesh *mesh, uint32_t index, MeshSectionC *out)
{
    const uint8_t *p = record(mesh, mesh->section_c_offset, mesh->section_c_count, SECTION_C_SIZE, index);
    if (p == NULL) return MESH_ERR_INDEX;
    memcpy(out->name, p, sizeof out->name);
    out->value = le_u32(p + 8);
    return MESH_OK;
}

MeshResult mesh_label(const Mesh *mesh, uint32_t index, MeshLabel *out)
{
    const uint8_t *p = record(mesh, mesh->label_offset, mesh->label_count, LABEL_SIZE, index);
    if (p == NULL) return MESH_ERR_INDEX;
    memcpy(out->name, p, sizeof out->name);
    out->polygon = le_u32(p + 8);
    return MESH_OK;
}

MeshResult mesh_vertex_map(const Mesh *mesh, uint32_t slot, uint32_t *out)
{
    if (slot >= mesh->vertex_map_slots) return MESH_ERR_INDEX;
    *out = le_u32(mesh->base + mesh->vertex_map_offset + (size_t)slot * SLOT_SIZE);
    return MESH_OK;
}

MeshResult mesh_polygon_corners(const Mesh *mesh, const MeshPolygon *polygon,
                                uint32_t vertices[4], uint32_t uvs[4])
{
    uint32_t v[POLYGON_CORNERS], t[POLYGON_CORNERS];
    for (uint32_t i = 0; i < POLYGON_CORNERS; i++) {
        if (polygon->vertex_slot > UINT32_MAX - i || polygon->uv_slot > UINT32_MAX - i) {
            return MESH_ERR_INDEX;
        }
        MeshResult r = mesh_vertex_map(mesh, polygon->vertex_slot + i, &v[i]);
        if (r == MESH_OK) r = mesh_vertex_map(mesh, polygon->uv_slot + i, &t[i]);
        if (r != MESH_OK) return r;
    }
    memcpy(vertices, v, sizeof v);
    memcpy(uvs, t, sizeof t);
    return MESH_OK;
}

const char *mesh_result_str(MeshResult result)
{
    switch (result) {
    case MESH_OK: return "ok";
    case MESH_ERR_KIND: return "not a mesh chunk";
    case MESH_ERR_BOUNDS: return "mesh table outside the chunk";
    case MESH_ERR_INDEX: return "mesh index out of range";
    }
    return "unknown error";
}
