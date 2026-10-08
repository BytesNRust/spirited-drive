#include "formats/tri.h"

#include <string.h>

enum {
    NODE_SIZE = 36,
    NODES_OFFSET = 0x98C,
    SEGMENT_TABLE_OFFSET = 0x2C,
    BRIGHTNESS_OFFSET = 0x15B0C,
    BRIGHTNESS_ENTRY = 3,
    DESCRIPTOR_COUNT_OFFSET = 0x16214,
    SCENERY_SLOTS_OFFSET = 0x16218,
    HEADER_BLOCK_SIZE = 0x1621C,
    CHUNK_HEADER_SIZE = 12,
    DESCRIPTOR_SIZE = 16,
    SCENERY_SIZE = 16,
    SEGMENT_CHUNK_SIZE = 288,
    SEGMENT_PAYLOAD = SEGMENT_CHUNK_SIZE - CHUNK_HEADER_SIZE,
    ROW_SIZE = 66
};

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static int16_t read_i16(const uint8_t *p)
{
    return (int16_t)read_u16(p);
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t read_i32(const uint8_t *p)
{
    return (int32_t)read_u32(p);
}

static TriPoint read_point(const uint8_t *p)
{
    TriPoint pt = { read_i16(p), read_i16(p + 2), read_i16(p + 4) };
    return pt;
}

static TriResult check_segments(const Tri *tri)
{
    for (uint32_t i = 0; i < tri->segment_count; i++) {
        const uint32_t offset = read_u32(tri->data + SEGMENT_TABLE_OFFSET + 4 * (size_t)i);
        if (offset > tri->terrain_size || tri->terrain_size - offset < SEGMENT_CHUNK_SIZE) {
            return TRI_ERR_BOUNDS;
        }
        const uint8_t *chunk = tri->data + tri->terrain_offset + offset;
        if (memcmp(chunk, "TRKD", 4) != 0) return TRI_ERR_CHUNK;
        if (read_u32(chunk + 4) < SEGMENT_PAYLOAD) return TRI_ERR_CHUNK;
    }
    return TRI_OK;
}

TriResult tri_open(Tri *tri, const uint8_t *data, size_t size)
{
    if (size < (size_t)HEADER_BLOCK_SIZE + CHUNK_HEADER_SIZE || data[0] != TRI_VERSION) {
        return TRI_ERR_HEADER;
    }
    Tri t;
    memset(&t, 0, sizeof t);
    t.data = data;
    t.size = size;
    t.circuit_segments = read_u16(data + 4);
    t.segment_count = read_u16(data + 6);
    t.node0_slope = read_u16(data + 0x18);
    t.node0_orientation = read_u16(data + 0x20);
    t.terrain_size = read_u32(data + 0x24);
    t.descriptor_count = read_u32(data + DESCRIPTOR_COUNT_OFFSET);
    t.scenery_slots = read_u32(data + SCENERY_SLOTS_OFFSET);
    if (t.segment_count > TRI_SEGMENT_CAPACITY) return TRI_ERR_HEADER;
    t.node_count = (uint32_t)t.segment_count * TRI_NODES_PER_SEGMENT;
    t.ring_segments = t.circuit_segments != 0 ? t.circuit_segments : TRI_OPEN_RING_SEGMENTS;
    t.ring_nodes = t.ring_segments * TRI_NODES_PER_SEGMENT;

    /* Object chunk: the next block starts at HEADER_BLOCK_SIZE + chunk_size. */
    if (memcmp(data + HEADER_BLOCK_SIZE, "SJBO", 4) != 0) return TRI_ERR_CHUNK;
    const uint32_t chunk_size = read_u32(data + HEADER_BLOCK_SIZE + 4);
    if (chunk_size < CHUNK_HEADER_SIZE || chunk_size > size - HEADER_BLOCK_SIZE) {
        return TRI_ERR_BOUNDS;
    }
    const size_t payload = chunk_size - CHUNK_HEADER_SIZE;
    if (t.descriptor_count > payload / DESCRIPTOR_SIZE) return TRI_ERR_BOUNDS;
    t.descriptors_offset = (size_t)HEADER_BLOCK_SIZE + CHUNK_HEADER_SIZE;
    t.scenery_offset = t.descriptors_offset + (size_t)t.descriptor_count * DESCRIPTOR_SIZE;
    t.terrain_offset = (size_t)HEADER_BLOCK_SIZE + chunk_size;

    /* 0043e4e0 scans up to 16000 bytes for the first record starting with -1. */
    size_t room = (t.descriptors_offset + payload - t.scenery_offset) / SCENERY_SIZE;
    if (room > TRI_SCENERY_MAX) room = TRI_SCENERY_MAX;
    while (t.scenery_count < room &&
           read_i32(data + t.scenery_offset + (size_t)t.scenery_count * SCENERY_SIZE) != -1) {
        t.scenery_count++;
    }

    if (t.terrain_size > size - t.terrain_offset) return TRI_ERR_BOUNDS;
    t.trailer_size = size - t.terrain_offset - t.terrain_size;
    const TriResult r = check_segments(&t);
    if (r != TRI_OK) return r;

    *tri = t;
    return TRI_OK;
}

int tri_is_circuit(const Tri *tri)
{
    return tri->circuit_segments != 0;
}

TriResult tri_node(const Tri *tri, uint32_t index, TriNode *out)
{
    if (index > tri->node_count || index >= TRI_NODE_CAPACITY) return TRI_ERR_BOUNDS;
    const uint8_t *p = tri->data + NODES_OFFSET + (size_t)index * NODE_SIZE;
    TriNode n;
    n.left_verge = p[0];
    n.right_verge = p[1];
    n.left_barrier = p[2];
    n.right_barrier = p[3];
    memcpy(n.flags, p + 4, 3);
    n.property = p[7];
    n.position.x = read_i32(p + 8);
    n.position.y = read_i32(p + 12);
    n.position.z = read_i32(p + 16);
    n.slope = read_i16(p + 0x14);
    n.slant_a = read_i16(p + 0x16);
    n.orientation = read_i16(p + 0x18);
    n.unknown_1a = read_u16(p + 0x1A);
    n.z_orientation = read_i16(p + 0x1C);
    n.slant_b = read_i16(p + 0x1E);
    n.x_orientation = read_i16(p + 0x20);
    n.unknown_22 = read_u16(p + 0x22);
    *out = n;
    return TRI_OK;
}

TriResult tri_descriptor(const Tri *tri, uint32_t index, TriDescriptor *out)
{
    if (index >= tri->descriptor_count) return TRI_ERR_BOUNDS;
    const uint8_t *p = tri->data + tri->descriptors_offset + (size_t)index * DESCRIPTOR_SIZE;
    TriDescriptor d;
    d.flags = p[0];
    d.type = p[1];
    d.resource_id = p[2];
    d.resource2_id = p[3];
    d.unknown_04 = read_u16(p + 4);
    d.width = read_i16(p + 6);
    d.unknown_08 = read_u16(p + 8);
    d.unknown_0a = read_u16(p + 0xA);
    d.unknown_0c = read_u16(p + 0xC);
    d.height = read_i16(p + 0xE);
    *out = d;
    return TRI_OK;
}

TriResult tri_scenery(const Tri *tri, uint32_t index, TriScenery *out)
{
    if (index >= tri->scenery_count) return TRI_ERR_BOUNDS;
    const uint8_t *p = tri->data + tri->scenery_offset + (size_t)index * SCENERY_SIZE;
    TriScenery s;
    s.node = read_i32(p);
    s.descriptor = p[4];
    s.orientation = p[5];
    memcpy(s.flags, p + 6, 4);
    s.x = read_i16(p + 0xA);
    s.y = read_i16(p + 0xC);
    s.z = read_i16(p + 0xE);
    *out = s;
    return TRI_OK;
}

TriResult tri_segment(const Tri *tri, uint32_t index, TriSegment *out)
{
    if (index >= tri->segment_count) return TRI_ERR_BOUNDS;
    /* Offsets and tags were validated by tri_open. */
    const uint32_t offset = read_u32(tri->data + SEGMENT_TABLE_OFFSET + 4 * (size_t)index);
    const uint8_t *p = tri->data + tri->terrain_offset + offset;
    TriSegment s;
    s.length = read_u32(p + 4);
    s.unknown_08 = read_u32(p + 8);
    s.unknown_0c = p[0xC];
    s.fence = p[0xD];
    memcpy(s.texture, p + 0xE, sizeof s.texture);
    for (int r = 0; r < 4; r++) {
        const uint8_t *row = p + 0x18 + r * ROW_SIZE;
        s.rows[r].middle = read_point(row);
        for (int i = 0; i < 5; i++) {
            s.rows[r].right[i] = read_point(row + 6 * (1 + i));
            s.rows[r].left[i] = read_point(row + 6 * (6 + i));
        }
    }
    *out = s;
    return TRI_OK;
}

TriResult tri_brightness(const Tri *tri, uint32_t index, uint8_t out[3])
{
    if (index >= TRI_SEGMENT_CAPACITY) return TRI_ERR_BOUNDS;
    memcpy(out, tri->data + BRIGHTNESS_OFFSET + (size_t)index * BRIGHTNESS_ENTRY, BRIGHTNESS_ENTRY);
    return TRI_OK;
}

uint8_t tri_brightness_scale(uint8_t value, uint32_t scale16)
{
    /* 0043e4e0: (v * scale) >> 16, plus 1 when bit 15 is set. */
    const uint32_t p = (uint32_t)value * scale16;
    return (uint8_t)((p >> 16) + ((p >> 15) & 1));
}

const char *tri_result_str(TriResult result)
{
    switch (result) {
    case TRI_OK: return "ok";
    case TRI_ERR_HEADER: return "bad TRI header";
    case TRI_ERR_BOUNDS: return "block or index outside the data";
    case TRI_ERR_CHUNK: return "unexpected chunk tag";
    }
    return "unknown error";
}
