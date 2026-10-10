#include "formats/doc.h"

#include "formats/le.h"

#include <stdlib.h>
#include <string.h>

enum {
    MAX_DEPTH = 8,          /* same nesting limit as chunk_survey */
    WWWW_COUNT_AT = 4,
    WWWW_TABLE_AT = 8,
    SHPI_SIZE_AT = 4,
    SHPI_COUNT_AT = 8,
    SHPI_TABLE_AT = 0x10,
    SHPI_STRIDE = 8,
    SHPI_OFFSET_AT = 4
};

typedef struct {
    uint32_t count_at, table_at, stride, offset_at;
    int has_size_word;
} Layout;

static const Layout wwww_layout = { WWWW_COUNT_AT, WWWW_TABLE_AT, 4, 0, 0 };
static const Layout shpi_layout = { SHPI_COUNT_AT, SHPI_TABLE_AT, SHPI_STRIDE, SHPI_OFFSET_AT, 1 };

struct DocNode {
    /* leaf */
    uint8_t *bytes;
    size_t bytes_size;
    /* container */
    int container;
    Layout layout;
    int patch_size_word;
    uint8_t *head;
    size_t head_size;
    uint32_t count;
    int32_t *entry_block;   /* per entry, -1 = absent */
    DocNode **blocks;
    uint32_t block_count;
};

static void put_u32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

static DocNode *new_leaf(const uint8_t *data, size_t size)
{
    DocNode *n = calloc(1, sizeof *n);
    if (n == NULL) return NULL;
    n->bytes = malloc(size ? size : 1);
    if (n->bytes == NULL) { free(n); return NULL; }
    memcpy(n->bytes, data, size);
    n->bytes_size = size;
    return n;
}

void doc_free(DocNode *node)
{
    if (node == NULL) return;
    free(node->bytes);
    free(node->head);
    free(node->entry_block);
    for (uint32_t i = 0; i < node->block_count; ++i) doc_free(node->blocks[i]);
    free(node->blocks);
    free(node);
}

static int cmp_u32(const void *a, const void *b)
{
    const uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}

static int is_wwww(const uint8_t *p, size_t size)
{
    return size >= WWWW_TABLE_AT && memcmp(p, "wwww", 4) == 0;
}

static DocResult parse_container(const uint8_t *data, size_t size, const Layout *layout, int depth, int chunks, DocNode **out, size_t *at);

/* A block's node: a nested container when it is a sound "wwww" tree, else a leaf. */
static DocNode *make_block(const uint8_t *data, size_t size, int depth, int chunks)
{
    if (chunks && depth < MAX_DEPTH && is_wwww(data, size)) {
        DocNode *nested = NULL;
        if (parse_container(data, size, &wwww_layout, depth + 1, 1, &nested, NULL) == DOC_OK) return nested;
    }
    return new_leaf(data, size);
}

static DocResult parse_container(const uint8_t *data, size_t size, const Layout *layout, int depth, int chunks, DocNode **out, size_t *at)
{
    if (size < layout->table_at + 4) { if (at) *at = 0; return DOC_ERR_BOUNDS; }
    const uint32_t count = le_u32(data + layout->count_at);
    const uint64_t table_end = (uint64_t)layout->table_at + (uint64_t)count * layout->stride;
    if (table_end > size) { if (at) *at = layout->count_at; return DOC_ERR_BOUNDS; }

    uint32_t *offsets = malloc((count ? count : 1) * sizeof *offsets);
    uint32_t *sorted = malloc((count ? count : 1) * sizeof *sorted);
    DocNode *n = calloc(1, sizeof *n);
    if (offsets == NULL || sorted == NULL || n == NULL) { free(offsets); free(sorted); free(n); return DOC_ERR_MEMORY; }
    DocResult r = DOC_OK;
    uint32_t distinct = 0;
    for (uint32_t i = 0; i < count && r == DOC_OK; ++i) {
        offsets[i] = le_u32(data + layout->table_at + (size_t)i * layout->stride + layout->offset_at);
        if (offsets[i] != 0 && (offsets[i] < table_end || offsets[i] >= size)) {
            r = DOC_ERR_BOUNDS;
            if (at) *at = layout->table_at + (size_t)i * layout->stride + layout->offset_at;
        }
        if (offsets[i] != 0) sorted[distinct++] = offsets[i];
    }
    if (r == DOC_OK) {
        qsort(sorted, distinct, sizeof *sorted, cmp_u32);
        uint32_t unique = 0;
        for (uint32_t i = 0; i < distinct; ++i)
            if (unique == 0 || sorted[i] != sorted[unique - 1]) sorted[unique++] = sorted[i];
        distinct = unique;

        n->container = 1;
        n->layout = *layout;
        n->count = count;
        n->head_size = distinct ? sorted[0] : size;
        n->head = malloc(n->head_size ? n->head_size : 1);
        n->entry_block = malloc((count ? count : 1) * sizeof *n->entry_block);
        n->blocks = calloc(distinct ? distinct : 1, sizeof *n->blocks);
        n->block_count = distinct;
        if (n->head == NULL || n->entry_block == NULL || n->blocks == NULL) r = DOC_ERR_MEMORY;
    }
    if (r == DOC_OK) {
        memcpy(n->head, data, n->head_size);
        n->patch_size_word = layout->has_size_word && size >= SHPI_SIZE_AT + 4 && le_u32(data + SHPI_SIZE_AT) == size;
        for (uint32_t i = 0; i < count; ++i) {
            n->entry_block[i] = -1;
            if (offsets[i] == 0) continue;
            for (uint32_t b = 0; b < distinct; ++b)
                if (sorted[b] == offsets[i]) { n->entry_block[i] = (int32_t)b; break; }
        }
        for (uint32_t b = 0; b < distinct && r == DOC_OK; ++b) {
            const size_t end = b + 1 < distinct ? sorted[b + 1] : size;
            n->blocks[b] = make_block(data + sorted[b], end - sorted[b], depth, chunks);
            if (n->blocks[b] == NULL) r = DOC_ERR_MEMORY;
        }
    }
    free(offsets);
    free(sorted);
    if (r != DOC_OK) { doc_free(n); return r; }
    *out = n;
    return DOC_OK;
}

DocResult doc_parse_chunks_at(const uint8_t *data, size_t size, DocNode **out, size_t *at)
{
    if (at) *at = 0;
    if (!is_wwww(data, size)) return DOC_ERR_HEADER;
    return parse_container(data, size, &wwww_layout, 0, 1, out, at);
}

DocResult doc_parse_chunks(const uint8_t *data, size_t size, DocNode **out)
{
    return doc_parse_chunks_at(data, size, out, NULL);
}

DocResult doc_parse_shpi_at(const uint8_t *data, size_t size, DocNode **out, size_t *at)
{
    if (at) *at = 0;
    if (size < SHPI_TABLE_AT || memcmp(data, "SHPI", 4) != 0) return DOC_ERR_HEADER;
    return parse_container(data, size, &shpi_layout, 0, 0, out, at);
}

DocResult doc_parse_shpi(const uint8_t *data, size_t size, DocNode **out)
{
    return doc_parse_shpi_at(data, size, out, NULL);
}

DocResult doc_expand_shpi(DocNode *leaf)
{
    if (leaf == NULL || leaf->container) return DOC_ERR_INDEX;
    DocNode *parsed = NULL;
    const DocResult r = doc_parse_shpi(leaf->bytes, leaf->bytes_size, &parsed);
    if (r != DOC_OK) return r;
    free(leaf->bytes);
    *leaf = *parsed;
    free(parsed);
    return DOC_OK;
}

int doc_is_container(const DocNode *node) { return node->container; }
uint32_t doc_entry_count(const DocNode *node) { return node->container ? node->count : 0; }

DocResult doc_entry(const DocNode *node, uint32_t index, DocNode **child)
{
    if (!node->container || index >= node->count) return DOC_ERR_INDEX;
    *child = node->entry_block[index] < 0 ? NULL : node->blocks[node->entry_block[index]];
    return DOC_OK;
}

const uint8_t *doc_bytes(const DocNode *leaf, size_t *size)
{
    if (leaf->container) return NULL;
    *size = leaf->bytes_size;
    return leaf->bytes;
}

uint8_t *doc_head(DocNode *node, size_t *size)
{
    if (!node->container) return NULL;
    *size = node->head_size;
    return node->head;
}

DocResult doc_set_bytes(DocNode *leaf, const uint8_t *bytes, size_t size)
{
    if (leaf->container) return DOC_ERR_INDEX;
    uint8_t *copy = malloc(size ? size : 1);
    if (copy == NULL) return DOC_ERR_MEMORY;
    memcpy(copy, bytes, size);
    free(leaf->bytes);
    leaf->bytes = copy;
    leaf->bytes_size = size;
    return DOC_OK;
}

size_t doc_size(const DocNode *node)
{
    if (!node->container) return node->bytes_size;
    size_t total = node->head_size;
    for (uint32_t b = 0; b < node->block_count; ++b) total += doc_size(node->blocks[b]);
    return total;
}

DocResult doc_write(const DocNode *node, uint8_t *dst, size_t cap, size_t *written)
{
    const size_t total = doc_size(node);
    if (total > cap) return DOC_ERR_CAPACITY;
    if (total > 0xFFFFFFFFu) return DOC_ERR_BOUNDS;
    if (!node->container) {
        memcpy(dst, node->bytes, node->bytes_size);
        *written = total;
        return DOC_OK;
    }
    memcpy(dst, node->head, node->head_size);
    size_t at = node->head_size;
    size_t *start = malloc((node->block_count ? node->block_count : 1) * sizeof *start);
    if (start == NULL) return DOC_ERR_MEMORY;
    DocResult r = DOC_OK;
    for (uint32_t b = 0; b < node->block_count && r == DOC_OK; ++b) {
        size_t len = 0;
        start[b] = at;
        r = doc_write(node->blocks[b], dst + at, cap - at, &len);
        at += len;
    }
    if (r == DOC_OK) {
        for (uint32_t i = 0; i < node->count; ++i) {
            if (node->entry_block[i] < 0) continue;
            put_u32(dst + node->layout.table_at + (size_t)i * node->layout.stride + node->layout.offset_at,
                    (uint32_t)start[node->entry_block[i]]);
        }
        if (node->patch_size_word) put_u32(dst + SHPI_SIZE_AT, (uint32_t)total);
        *written = total;
    }
    free(start);
    return r;
}

const char *doc_result_str(DocResult result)
{
    switch (result) {
    case DOC_OK: return "ok";
    case DOC_ERR_BOUNDS: return "table or offset outside the data";
    case DOC_ERR_HEADER: return "not the expected container tag";
    case DOC_ERR_INDEX: return "entry out of range or not a container";
    case DOC_ERR_MEMORY: return "out of memory";
    case DOC_ERR_CAPACITY: return "destination too small";
    case DOC_ERR_DEPTH: return "containers nested too deep";
    }
    return "unknown error";
}
