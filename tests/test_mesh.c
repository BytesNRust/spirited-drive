/* ORIP mesh tests on a synthetic chunk laid out like the real ones (see
 * formats/mesh.h): table bounds, record accessors and the vertex map. */
#include "formats/mesh.h"
#include "test.h"

#include <stdint.h>

/* Table offsets of the sample, all after the 0x70 header. */
enum {
    VERTEX_AT = 0x70, UV_AT = VERTEX_AT + 3 * 12, POLY_AT = UV_AT + 2 * 8,
    TEXTURE_AT = POLY_AT + 2 * 12, A_AT = TEXTURE_AT + 2 * 20, B_AT = A_AT + 20,
    C_AT = B_AT + 28, LABEL_AT = C_AT + 12, MAP_AT = LABEL_AT + 12,
    MAP_SLOTS = 8, SAMPLE_SIZE = MAP_AT + MAP_SLOTS * 4
};

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_table(uint8_t *b, unsigned count_at, uint32_t count, uint32_t offset_at)
{
    put_u32(b + count_at, count);
    put_u32(b + count_at + 4, offset_at);
}

static void build_sample(uint8_t b[SAMPLE_SIZE])
{
    memset(b, 0, SAMPLE_SIZE);
    memcpy(b, "ORIP", 4);
    put_u32(b + 0x04, SAMPLE_SIZE);
    put_u32(b + 0x08, 0x2BC);
    put_table(b, 0x10, 3, 0);              /* count at 0x10, offset at 0x14 is overwritten below */
    put_u32(b + 0x14, 2);                  /* unknown_14 */
    put_u32(b + 0x18, VERTEX_AT);
    put_table(b, 0x1C, 2, UV_AT);
    put_table(b, 0x24, 2, POLY_AT);
    memcpy(b + 0x2C, "_TEST", 5);
    put_table(b, 0x38, 2, TEXTURE_AT);
    put_table(b, 0x40, 1, A_AT);
    put_table(b, 0x48, 1, B_AT);
    put_u32(b + 0x50, MAP_AT);
    put_table(b, 0x54, 1, C_AT);
    put_table(b, 0x5C, 1, LABEL_AT);
    put_u32(b + 0x68, POLY_AT);

    put_u32(b + VERTEX_AT + 12 * 2, (uint32_t)-5);     /* vertex 2: x = -5 */
    put_u32(b + VERTEX_AT + 12 * 2 + 8, 70000);
    put_u32(b + UV_AT + 8, 12);
    put_u32(b + UV_AT + 12, (uint32_t)-1);

    b[POLY_AT] = 0x84;                    /* quad, flag bit 7 */
    b[POLY_AT + 2] = 1;
    put_u32(b + POLY_AT + 4, 0);          /* vertex slots 0..3, uv slots 4..7 */
    put_u32(b + POLY_AT + 8, 4);
    b[POLY_AT + 12] = 0x03;               /* triangle without uvs */
    put_u32(b + POLY_AT + 16, 4);
    put_u32(b + POLY_AT + 20, 4);

    memcpy(b + TEXTURE_AT + 8, "topv", 4);
    put_u32(b + TEXTURE_AT + 16, 0xEF);
    memcpy(b + A_AT, "left_tur", 8);
    put_u32(b + A_AT + 16, 109);
    memcpy(b + B_AT, "surface", 7);
    put_u32(b + B_AT + 24, 0x3D);
    memcpy(b + C_AT, "smok", 4);
    put_u32(b + C_AT + 8, 24);
    memcpy(b + LABEL_AT, "bklr", 4);
    put_u32(b + LABEL_AT + 8, 1);

    const uint32_t map[MAP_SLOTS] = { 0, 1, 2, 0, 1, 0, 1, 2 };
    for (int i = 0; i < MAP_SLOTS; i++) put_u32(b + MAP_AT + 4 * i, map[i]);
}

static Chunk mesh_chunk(const uint8_t *b, size_t size)
{
    Chunk c = { CHUNK_KIND_MESH, b, size, 0 };
    return c;
}

static void test_records(void)
{
    uint8_t b[SAMPLE_SIZE];
    build_sample(b);
    const Chunk chunk = mesh_chunk(b, sizeof b);
    Mesh m;
    CHECK_EQ_INT(mesh_open(&m, &chunk), MESH_OK);
    CHECK_MEM(m.name, "_TEST", 6);
    CHECK_EQ_INT(m.unknown_08, 0x2BC);
    CHECK_EQ_INT(m.unknown_14, 2);
    CHECK_EQ_INT(m.vertex_count, 3);
    CHECK_EQ_INT(m.polygon_count, 2);
    CHECK_EQ_INT(m.vertex_map_slots, MAP_SLOTS);

    MeshVertex v;
    CHECK_EQ_INT(mesh_vertex(&m, 2, &v), MESH_OK);
    CHECK_EQ_INT(v.x, -5);
    CHECK_EQ_INT(v.z, 70000);
    CHECK_EQ_INT(mesh_vertex(&m, 3, &v), MESH_ERR_INDEX);

    MeshUv uv;
    CHECK_EQ_INT(mesh_uv(&m, 1, &uv), MESH_OK);
    CHECK_EQ_INT(uv.u, 12);
    CHECK_EQ_INT(uv.v, -1);

    MeshPolygon p;
    uint32_t verts[4], uvs[4];
    CHECK_EQ_INT(mesh_polygon(&m, 0, &p), MESH_OK);
    CHECK_EQ_INT(p.flags & 7, 4);
    CHECK_EQ_INT(p.texture, 1);
    CHECK_EQ_INT(mesh_polygon_corners(&m, &p, verts, uvs), MESH_OK);
    CHECK_EQ_INT(verts[2], 2);
    CHECK_EQ_INT(uvs[1], 0);
    CHECK_EQ_INT(uvs[3], 2);
    CHECK_EQ_INT(mesh_polygon(&m, 1, &p), MESH_OK);
    CHECK_EQ_INT(p.flags, 3);
    CHECK_EQ_INT(p.vertex_slot, p.uv_slot);
    CHECK_EQ_INT(mesh_polygon(&m, 2, &p), MESH_ERR_INDEX);

    MeshTexture t;
    CHECK_EQ_INT(mesh_texture(&m, 0, &t), MESH_OK);
    CHECK_MEM(t.name, "topv", 4);
    CHECK_EQ_INT(t.unknown_1[1], 0xEF);

    MeshSectionA a;
    MeshSectionB sb;
    MeshSectionC c;
    MeshLabel l;
    CHECK_EQ_INT(mesh_section_a(&m, 0, &a), MESH_OK);
    CHECK_MEM(a.name, "left_tur", 8);
    CHECK_EQ_INT(a.unknown[2], 109);
    CHECK_EQ_INT(mesh_section_b(&m, 0, &sb), MESH_OK);
    CHECK_MEM(sb.name, "surface", 7);
    CHECK_EQ_INT(sb.unknown[3], 0x3D);
    CHECK_EQ_INT(mesh_section_c(&m, 0, &c), MESH_OK);
    CHECK_EQ_INT(c.value, 24);
    CHECK_EQ_INT(mesh_label(&m, 0, &l), MESH_OK);
    CHECK_MEM(l.name, "bklr", 4);
    CHECK_EQ_INT(l.polygon, 1);
    CHECK_EQ_INT(mesh_label(&m, 1, &l), MESH_ERR_INDEX);
}

static void test_bounds(void)
{
    uint8_t b[SAMPLE_SIZE];
    Mesh m;
    Chunk chunk = mesh_chunk(b, sizeof b);

    /* A chunk cut anywhere before the end of its last table is refused. */
    build_sample(b);
    for (size_t len = 0; len < MAP_AT; len++) {
        chunk = mesh_chunk(b, len);
        CHECK(mesh_open(&m, &chunk) != MESH_OK);
    }

    build_sample(b);
    chunk.kind = CHUNK_KIND_BITMAP;
    CHECK_EQ_INT(mesh_open(&m, &chunk), MESH_ERR_KIND);
    chunk = mesh_chunk(b, sizeof b);

    put_u32(b + 0x10, 0xFFFFFFFFu);        /* vertex count overflowing 32 bits */
    CHECK_EQ_INT(mesh_open(&m, &chunk), MESH_ERR_BOUNDS);
    build_sample(b);
    put_u32(b + 0x18, 0xFFFFFFF8u);        /* offset near 2^32 */
    CHECK_EQ_INT(mesh_open(&m, &chunk), MESH_ERR_BOUNDS);
    build_sample(b);
    put_u32(b + 0x28, 0x10);               /* table inside the header */
    CHECK_EQ_INT(mesh_open(&m, &chunk), MESH_ERR_BOUNDS);
    build_sample(b);
    put_u32(b + 0x50, SAMPLE_SIZE + 4);    /* vertex map past the end */
    CHECK_EQ_INT(mesh_open(&m, &chunk), MESH_ERR_BOUNDS);

    /* A slot run that starts inside the map but ends outside it. */
    build_sample(b);
    CHECK_EQ_INT(mesh_open(&m, &chunk), MESH_OK);
    MeshPolygon p;
    uint32_t verts[4], uvs[4], raw;
    mesh_polygon(&m, 0, &p);
    p.uv_slot = MAP_SLOTS - 3;
    CHECK_EQ_INT(mesh_polygon_corners(&m, &p, verts, uvs), MESH_ERR_INDEX);
    p.uv_slot = 0xFFFFFFFFu;
    CHECK_EQ_INT(mesh_polygon_corners(&m, &p, verts, uvs), MESH_ERR_INDEX);
    CHECK_EQ_INT(mesh_vertex_map(&m, MAP_SLOTS, &raw), MESH_ERR_INDEX);
    CHECK_EQ_INT(mesh_vertex_map(&m, MAP_SLOTS - 1, &raw), MESH_OK);
    CHECK_EQ_INT(raw, 2);
}

int main(void)
{
    test_records();
    test_bounds();
    return TEST_REPORT();
}
