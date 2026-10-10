#include "formats/tri.h"

#include <string.h>

enum {
    SEGMENT_COUNT_OFFSET = 6,
    TERRAIN_SIZE_OFFSET = 0x24,
    SEGMENT_TABLE_OFFSET = 0x2C,
    DESCRIPTOR_COUNT_OFFSET = 0x16214,
    CHUNK_HEADER_SIZE = 12,
    SCENERY_AREA = TRI_SCENERY_MAX * TRI_SCENERY_SIZE
};

static void put_u32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

TriResult tri_parts(const Tri *tri, TriParts *parts)
{
    const size_t descriptor_bytes = (size_t)tri->descriptor_count * TRI_DESCRIPTOR_SIZE;
    if (tri->scenery_offset != tri->descriptors_offset + descriptor_bytes) return TRI_ERR_BOUNDS;
    if (tri->terrain_offset - tri->scenery_offset != SCENERY_AREA) return TRI_ERR_BOUNDS;
    parts->header = tri->data;
    parts->descriptors = tri->data + tri->descriptors_offset;
    parts->descriptor_count = tri->descriptor_count;
    parts->scenery = tri->data + tri->scenery_offset;
    parts->terrain = tri->data + tri->terrain_offset;
    parts->segment_count = tri->segment_count;
    parts->trailer = tri->data + tri->terrain_offset + tri->terrain_size;
    parts->trailer_size = tri->trailer_size;
    return TRI_OK;
}

size_t tri_write_size(const TriParts *parts)
{
    return (size_t)TRI_HEADER_SIZE + CHUNK_HEADER_SIZE + (size_t)parts->descriptor_count * TRI_DESCRIPTOR_SIZE +
           SCENERY_AREA + (size_t)parts->segment_count * TRI_SEGMENT_SIZE + parts->trailer_size;
}

TriResult tri_write(const TriParts *parts, uint8_t *dst, size_t cap, size_t *len)
{
    if (parts->segment_count > TRI_SEGMENT_CAPACITY) return TRI_ERR_BOUNDS;
    const size_t total = tri_write_size(parts);
    if (total > cap) return TRI_ERR_BOUNDS;
    const uint32_t terrain_size = parts->segment_count * TRI_SEGMENT_SIZE;
    const uint32_t chunk_size = CHUNK_HEADER_SIZE + parts->descriptor_count * TRI_DESCRIPTOR_SIZE + SCENERY_AREA;

    memcpy(dst, parts->header, TRI_HEADER_SIZE);
    dst[SEGMENT_COUNT_OFFSET] = (uint8_t)parts->segment_count;
    dst[SEGMENT_COUNT_OFFSET + 1] = (uint8_t)(parts->segment_count >> 8);
    put_u32(dst + TERRAIN_SIZE_OFFSET, terrain_size);
    for (uint32_t i = 0; i < TRI_SEGMENT_CAPACITY; ++i) put_u32(dst + SEGMENT_TABLE_OFFSET + 4 * i, i * TRI_SEGMENT_SIZE);
    put_u32(dst + DESCRIPTOR_COUNT_OFFSET, parts->descriptor_count);

    uint8_t *at = dst + TRI_HEADER_SIZE;
    memcpy(at, "SJBO", 4);
    put_u32(at + 4, chunk_size);
    put_u32(at + 8, 0);
    at += CHUNK_HEADER_SIZE;
    memcpy(at, parts->descriptors, (size_t)parts->descriptor_count * TRI_DESCRIPTOR_SIZE);
    at += (size_t)parts->descriptor_count * TRI_DESCRIPTOR_SIZE;
    memcpy(at, parts->scenery, SCENERY_AREA);
    at += SCENERY_AREA;
    memcpy(at, parts->terrain, terrain_size);
    at += terrain_size;
    if (parts->trailer_size > 0) memcpy(at, parts->trailer, parts->trailer_size);
    *len = total;
    return TRI_OK;
}
