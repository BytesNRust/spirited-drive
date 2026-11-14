/* TRI: a track's road, scenery and terrain */
#include "fuzz/fuzz.h"
#include "formats/tri.h"

#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Tri tri;
    if (tri_open(&tri, data, size) != TRI_OK) return 0;
    (void)tri_is_circuit(&tri);
    for (uint32_t i = 0; i < tri.node_count + 4; i++) { TriNode n; (void)tri_node(&tri, i, &n); }
    for (uint32_t i = 0; i < tri.descriptor_count + 4; i++) { TriDescriptor d; (void)tri_descriptor(&tri, i, &d); }
    for (uint32_t i = 0; i < tri.scenery_count + 4; i++) { TriScenery s; (void)tri_scenery(&tri, i, &s); }
    for (uint32_t i = 0; i < tri.segment_count + 4; i++) {
        TriSegment s;
        uint8_t b[3];
        (void)tri_segment(&tri, i, &s);
        (void)tri_brightness(&tri, i, b);
    }
    TriParts parts;
    if (tri_parts(&tri, &parts) != TRI_OK) return 0;
    const size_t need = tri_write_size(&parts);
    uint8_t *out = need ? malloc(need) : NULL;
    size_t len = 0;
    if (out != NULL) (void)tri_write(&parts, out, need, &len), free(out);
    return 0;
}
