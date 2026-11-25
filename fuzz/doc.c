/* The document tree of chunk and SHPI files (the studio's editor and its exports) */
#include "fuzz/fuzz.h"
#include "formats/doc.h"

#include <stdlib.h>

enum { DEPTH = 8, ENTRIES = 4096 };

static void walk(DocNode *n, int depth)
{
    if (!doc_is_container(n)) {
        size_t len = 0;
        const uint8_t *const bytes = doc_bytes(n, &len);   /* the length is read after the call that sets it */
        fuzz_touch(bytes, len);
        return;
    }
    if (depth >= DEPTH) return;
    for (uint32_t i = 0; i < doc_entry_count(n) && i < ENTRIES; i++) {
        DocNode *child = NULL;
        if (doc_entry(n, i, &child) == DOC_OK && child != NULL) walk(child, depth + 1);
    }
}

static void round_trip(DocNode *n)
{
    walk(n, 0);
    const size_t need = doc_size(n);
    uint8_t *out = need && need < (64u << 20) ? malloc(need) : NULL;
    size_t len = 0;
    if (out != NULL) (void)doc_write(n, out, need, &len), free(out);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    DocNode *n = NULL;
    if (doc_parse_chunks(data, size, &n) == DOC_OK && n != NULL) round_trip(n);
    doc_free(n);
    n = NULL;
    if (doc_parse_shpi(data, size, &n) == DOC_OK && n != NULL) round_trip(n);
    doc_free(n);
    return 0;
}
