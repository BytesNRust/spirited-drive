/* QFS (refpack): every packed art file and resource_load's unpacking */
#include "fuzz/fuzz.h"
#include "formats/qfs.h"

#include <stdlib.h>

enum { CAP = 1 << 24 };   /* the biggest unpacked file the harness lets through */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    size_t out = 0;
    if (!qfs_is_compressed(data, size) || qfs_decompressed_size(data, size, &out) != QFS_OK || out > CAP) return 0;
    uint8_t *dst = malloc(out ? out : 1);
    if (dst == NULL) return 0;
    if (qfs_decompress(data, size, dst, out) == QFS_OK) fuzz_touch(dst, out);
    size_t at = 0;
    (void)qfs_decompress_at(data, size, dst, out, &at);
    (void)qfs_inplace_ok(data, size, out);
    free(dst);
    return 0;
}
