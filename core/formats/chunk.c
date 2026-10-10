#include "formats/chunk.h"

#include "formats/le.h"

#include <string.h>

enum {
    TAG_SIZE = 4,
    LEAF_HEADER_SIZE = 8,   /* tag + u32 length */
    TABLE_START = 8,        /* tag + u32 count, then the offsets */
    OFFSET_SIZE = 4
};

/* Reads the chunk that starts at data, with avail bytes left in the parent. */
static ChunkResult parse_at(Chunk *out, const uint8_t *data, size_t avail)
{
    if (avail < TAG_SIZE) return CHUNK_ERR_BOUNDS;
    ChunkKind kind;
    if (memcmp(data, "wwww", TAG_SIZE) == 0) kind = CHUNK_KIND_HEADER;
    else if (memcmp(data, "ORIP", TAG_SIZE) == 0) kind = CHUNK_KIND_MESH;
    else if (memcmp(data, "SHPI", TAG_SIZE) == 0) kind = CHUNK_KIND_BITMAP;
    else return CHUNK_ERR_TAG;
    if (avail < LEAF_HEADER_SIZE) return CHUNK_ERR_BOUNDS;

    const uint32_t word = le_u32(data + TAG_SIZE);
    Chunk chunk = { kind, data, avail, 0 };
    if (kind == CHUNK_KIND_HEADER) {
        if (word > (avail - TABLE_START) / OFFSET_SIZE) return CHUNK_ERR_BOUNDS;
        chunk.count = word;
    } else {
        if (word < LEAF_HEADER_SIZE || word > avail) return CHUNK_ERR_BOUNDS;
        chunk.size = word;
    }
    *out = chunk;
    return CHUNK_OK;
}

ChunkResult chunk_open(Chunk *out, const uint8_t *data, size_t size)
{
    return parse_at(out, data, size);
}

ChunkResult chunk_child(const Chunk *header, uint32_t index, Chunk *out)
{
    if (header->kind != CHUNK_KIND_HEADER) return CHUNK_ERR_KIND;
    if (index >= header->count) return CHUNK_ERR_INDEX;

    const uint8_t *table = header->data + TABLE_START;
    const uint32_t offset = le_u32(table + (size_t)index * OFFSET_SIZE);
    if (offset == 0) {
        const Chunk absent = { CHUNK_KIND_ABSENT, NULL, 0, 0 };
        *out = absent;
        return CHUNK_OK;
    }
    const size_t table_end = TABLE_START + (size_t)header->count * OFFSET_SIZE;
    if (offset < table_end || offset >= header->size) return CHUNK_ERR_BOUNDS;

    /* The extent ends at the next larger sibling offset. */
    size_t end = header->size;
    for (uint32_t i = 0; i < header->count; i++) {
        const uint32_t other = le_u32(table + (size_t)i * OFFSET_SIZE);
        if (other > offset && other < end) end = other;
    }
    return parse_at(out, header->data + offset, end - offset);
}

static ChunkResult survey_at(const Chunk *chunk, uint32_t depth, ChunkTotals *totals)
{
    if (depth > CHUNK_MAX_DEPTH) return CHUNK_ERR_DEPTH;
    if (depth > totals->max_depth) totals->max_depth = depth;
    switch (chunk->kind) {
    case CHUNK_KIND_ABSENT: totals->absent++; return CHUNK_OK;
    case CHUNK_KIND_MESH: totals->meshes++; return CHUNK_OK;
    case CHUNK_KIND_BITMAP: totals->bitmaps++; return CHUNK_OK;
    case CHUNK_KIND_HEADER: break;
    }
    totals->headers++;
    for (uint32_t i = 0; i < chunk->count; i++) {
        Chunk child;
        ChunkResult r = chunk_child(chunk, i, &child);
        if (r == CHUNK_OK) r = survey_at(&child, depth + 1, totals);
        if (r != CHUNK_OK) return r;
    }
    return CHUNK_OK;
}

ChunkResult chunk_survey(const Chunk *root, ChunkTotals *totals)
{
    const ChunkTotals zero = { 0, 0, 0, 0, 0 };
    *totals = zero;
    return survey_at(root, 0, totals);
}

FshResult chunk_bitmaps(const Chunk *bitmap, Fsh *out)
{
    if (bitmap->kind != CHUNK_KIND_BITMAP) return FSH_ERR_HEADER;
    return fsh_open(out, bitmap->data, bitmap->size);
}

const char *chunk_result_str(ChunkResult result)
{
    switch (result) {
    case CHUNK_OK: return "ok";
    case CHUNK_ERR_BOUNDS: return "chunk outside the data";
    case CHUNK_ERR_TAG: return "unknown chunk tag";
    case CHUNK_ERR_INDEX: return "child index out of range";
    case CHUNK_ERR_KIND: return "wrong chunk kind";
    case CHUNK_ERR_DEPTH: return "chunks nested too deep";
    }
    return "unknown error";
}
