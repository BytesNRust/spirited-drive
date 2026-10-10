#include "formats/bnk.h"

#include "formats/le.h"

#include <stdlib.h>
#include <string.h>

enum {
    EACS_OFFSET = 0x28,
    BIT_DEPTH_OFFSET = 0x30,
    CHANNELS_OFFSET = 0x31
};

BnkResult bnk_open(Bnk *bnk, const uint8_t *data, size_t size)
{
    if (size < BNK_TABLE_SIZE) return BNK_ERR_TRUNCATED;
    bnk->data = data;
    bnk->size = size;
    return BNK_OK;
}

static uint32_t slot_offset(const Bnk *bnk, int slot)
{
    return le_u32(bnk->data + 4 * slot);
}

BnkResult bnk_sample(const Bnk *bnk, int slot, BnkSample *out)
{
    if (slot < 0 || slot >= BNK_SLOTS) return BNK_ERR_SLOT;
    const uint32_t at = slot_offset(bnk, slot);
    if (at == 0) return BNK_ABSENT;
    if (at > bnk->size || bnk->size - at < BNK_HEADER_SIZE) return BNK_ERR_BOUNDS;
    const uint8_t *h = bnk->data + at;
    if (memcmp(h + EACS_OFFSET, "EACS", 4) != 0 || le_u32(h + 4) != at + EACS_OFFSET) return BNK_ERR_HEADER;
    if ((h[BIT_DEPTH_OFFSET] != 1 && h[BIT_DEPTH_OFFSET] != 2) || (h[CHANNELS_OFFSET] != 1 && h[CHANNELS_OFFSET] != 2))
        return BNK_ERR_HEADER;
    memcpy(out->header, h, BNK_HEADER_SIZE);
    out->header_offset = at;
    out->flags = le_u32(h);
    out->rate = le_u32(h + 0x2c);
    out->bits = 8 * h[BIT_DEPTH_OFFSET];
    out->channels = h[CHANNELS_OFFSET];
    out->loop_start = le_u32(h + 0x34);
    out->loop_length = le_u32(h + 0x38);
    out->length = le_u32(h + 0x3c);
    out->data_offset = le_u32(h + 0x40);
    const uint64_t end = (uint64_t)out->data_offset + (uint64_t)out->length * (uint64_t)bnk_frame_bytes(out);
    if (out->data_offset > bnk->size || end > bnk->size) return BNK_ERR_DATA;
    return BNK_OK;
}

int bnk_frame_bytes(const BnkSample *sample)
{
    return sample->bits / 8 * sample->channels;
}

size_t bnk_data_extent(const Bnk *bnk, const BnkSample *sample)
{
    size_t next = bnk->size;
    for (int slot = 0; slot < BNK_SLOTS; ++slot) {
        BnkSample other;
        const BnkResult r = bnk_sample(bnk, slot, &other);
        if (r != BNK_OK && r != BNK_ERR_DATA) continue;
        if (other.data_offset > sample->data_offset && other.data_offset < next) next = other.data_offset;
    }
    return next - sample->data_offset;
}

const char *bnk_result_str(BnkResult result)
{
    switch (result) {
    case BNK_OK: return "ok";
    case BNK_ABSENT: return "no sample in this slot";
    case BNK_ERR_TRUNCATED: return "file shorter than the 512-byte offset table";
    case BNK_ERR_SLOT: return "slot outside 0..127";
    case BNK_ERR_BOUNDS: return "sample header outside the file";
    case BNK_ERR_HEADER: return "bad sample header (EACS marker, +4 field, bit depth or channels)";
    case BNK_ERR_DATA: return "sample data runs past the end of the file";
    case BNK_ERR_MEMORY: return "out of memory";
    case BNK_ERR_CAPACITY: return "destination too small";
    case BNK_ERR_INDEX: return "block outside the bank";
    }
    return "unknown";
}

struct BnkDoc {
    uint8_t *head;
    size_t head_size;
    int slot_block[BNK_SLOTS];  /* -1 = absent */
    int block_count;
    uint8_t **block;
    size_t *block_size;
};

static int cmp_offset(const void *a, const void *b)
{
    const uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}

void bnk_doc_free(BnkDoc *doc)
{
    if (doc == NULL) return;
    for (int b = 0; b < doc->block_count; ++b) free(doc->block[b]);
    free(doc->block);
    free(doc->block_size);
    free(doc->head);
    free(doc);
}

static BnkResult fill_blocks(BnkDoc *doc, const uint8_t *data, size_t size, const uint32_t *starts, int n)
{
    doc->block = calloc((size_t)(n ? n : 1), sizeof *doc->block);
    doc->block_size = calloc((size_t)(n ? n : 1), sizeof *doc->block_size);
    if (doc->block == NULL || doc->block_size == NULL) return BNK_ERR_MEMORY;
    doc->block_count = n;
    for (int b = 0; b < n; ++b) {
        const size_t end = b + 1 < n ? starts[b + 1] : size;
        doc->block_size[b] = end - starts[b];
        doc->block[b] = malloc(doc->block_size[b] ? doc->block_size[b] : 1);
        if (doc->block[b] == NULL) return BNK_ERR_MEMORY;
        memcpy(doc->block[b], data + starts[b], doc->block_size[b]);
    }
    return BNK_OK;
}

BnkResult bnk_doc_read(const uint8_t *data, size_t size, BnkDoc **out)
{
    Bnk bnk;
    BnkResult r = bnk_open(&bnk, data, size);
    if (r != BNK_OK) return r;
    uint32_t starts[BNK_SLOTS];
    uint32_t data_at[BNK_SLOTS];
    uint32_t header_end = 0;
    int n = 0;
    for (int slot = 0; slot < BNK_SLOTS; ++slot) {
        BnkSample s;
        r = bnk_sample(&bnk, slot, &s);
        if (r == BNK_ABSENT) { data_at[slot] = 0; continue; }
        if (r != BNK_OK && r != BNK_ERR_DATA) return r;  /* a claim past the end is the caller's to fix */
        if (s.data_offset < BNK_TABLE_SIZE || s.data_offset >= size) return BNK_ERR_BOUNDS;
        data_at[slot] = s.data_offset;
        starts[n++] = s.data_offset;
        if (s.header_offset + BNK_HEADER_SIZE > header_end) header_end = s.header_offset + BNK_HEADER_SIZE;
    }
    qsort(starts, (size_t)n, sizeof *starts, cmp_offset);
    int distinct = 0;
    for (int i = 0; i < n; ++i)
        if (distinct == 0 || starts[i] != starts[distinct - 1]) starts[distinct++] = starts[i];
    if (distinct == 0 || header_end > starts[0]) return BNK_ERR_BOUNDS;

    BnkDoc *doc = calloc(1, sizeof *doc);
    if (doc == NULL) return BNK_ERR_MEMORY;
    doc->head_size = starts[0];
    doc->head = malloc(doc->head_size);
    if (doc->head == NULL) { bnk_doc_free(doc); return BNK_ERR_MEMORY; }
    memcpy(doc->head, data, doc->head_size);
    for (int slot = 0; slot < BNK_SLOTS; ++slot) {
        doc->slot_block[slot] = -1;
        for (int b = 0; data_at[slot] != 0 && b < distinct; ++b)
            if (starts[b] == data_at[slot]) { doc->slot_block[slot] = b; break; }
    }
    r = fill_blocks(doc, data, size, starts, distinct);
    if (r != BNK_OK) { bnk_doc_free(doc); return r; }
    *out = doc;
    return BNK_OK;
}

int bnk_doc_block_count(const BnkDoc *doc) { return doc->block_count; }

int bnk_doc_slot_block(const BnkDoc *doc, int slot)
{
    return slot < 0 || slot >= BNK_SLOTS ? -1 : doc->slot_block[slot];
}

const uint8_t *bnk_doc_block(const BnkDoc *doc, int block, size_t *size)
{
    if (block < 0 || block >= doc->block_count) return NULL;
    *size = doc->block_size[block];
    return doc->block[block];
}

BnkResult bnk_doc_set_block(BnkDoc *doc, int block, const uint8_t *pcm, size_t size)
{
    if (block < 0 || block >= doc->block_count) return BNK_ERR_INDEX;
    uint8_t *copy = malloc(size ? size : 1);
    if (copy == NULL) return BNK_ERR_MEMORY;
    memcpy(copy, pcm, size);
    free(doc->block[block]);
    doc->block[block] = copy;
    doc->block_size[block] = size;
    return BNK_OK;
}

uint8_t *bnk_doc_header(BnkDoc *doc, int slot)
{
    if (slot < 0 || slot >= BNK_SLOTS || doc->slot_block[slot] < 0) return NULL;
    return doc->head + le_u32(doc->head + 4 * slot);
}

size_t bnk_doc_size(const BnkDoc *doc)
{
    size_t total = doc->head_size;
    for (int b = 0; b < doc->block_count; ++b) total += doc->block_size[b];
    return total;
}

BnkResult bnk_doc_write(const BnkDoc *doc, uint8_t *dst, size_t cap, size_t *len)
{
    const size_t total = bnk_doc_size(doc);
    if (total > cap) return BNK_ERR_CAPACITY;
    if (total > 0xFFFFFFFFu) return BNK_ERR_BOUNDS;
    memcpy(dst, doc->head, doc->head_size);
    size_t at = doc->head_size;
    size_t start[BNK_SLOTS];
    for (int b = 0; b < doc->block_count; ++b) {
        start[b] = at;
        memcpy(dst + at, doc->block[b], doc->block_size[b]);
        at += doc->block_size[b];
    }
    for (int slot = 0; slot < BNK_SLOTS; ++slot) {
        if (doc->slot_block[slot] < 0) continue;
        uint8_t *field = dst + le_u32(doc->head + 4 * slot) + 0x40;
        const uint32_t v = (uint32_t)start[doc->slot_block[slot]];
        for (int i = 0; i < 4; ++i) field[i] = (uint8_t)(v >> (8 * i));
    }
    *len = total;
    return BNK_OK;
}
