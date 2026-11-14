/* BNK: the sound banks (engines, menus, crashes) */
#include "fuzz/fuzz.h"
#include "formats/bnk.h"

#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Bnk bnk;
    if (bnk_open(&bnk, data, size) == BNK_OK) {
        for (int slot = 0; slot < BNK_SLOTS; slot++) {
            BnkSample s;
            if (bnk_sample(&bnk, slot, &s) != BNK_OK) continue;
            (void)bnk_frame_bytes(&s);
            const size_t extent = bnk_data_extent(&bnk, &s);
            if (s.data_offset <= size && extent <= size - s.data_offset) fuzz_touch(data + s.data_offset, extent);
        }
    }
    BnkDoc *doc = NULL;
    if (bnk_doc_read(data, size, &doc) != BNK_OK) return 0;
    for (int b = 0; b < bnk_doc_block_count(doc); b++) { size_t n = 0; const uint8_t *p = bnk_doc_block(doc, b, &n); fuzz_touch(p, n); }
    for (int slot = 0; slot < BNK_SLOTS; slot++) (void)bnk_doc_slot_block(doc, slot);
    const size_t need = bnk_doc_size(doc);
    uint8_t *out = need && need < (64u << 20) ? malloc(need) : NULL;
    size_t len = 0;
    if (out != NULL) (void)bnk_doc_write(doc, out, need, &len), free(out);
    bnk_doc_free(doc);
    return 0;
}
