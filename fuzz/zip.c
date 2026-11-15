/* zip: the archive a mod is installed from (dep59), its directory and every entry unpacked */
#include "fuzz/fuzz.h"
#include "formats/zip.h"

#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ZipArchive z;
    if (zip_open(data, size, &z) != ZIP_OK) return 0;
    for (size_t i = 0; i < z.count; i++) {
        uint8_t *out = NULL;
        size_t n = 0;
        if (zip_extract(&z, i, &out, &n) == ZIP_OK) fuzz_touch(out, n);
        free(out);
    }
    zip_close(&z);
    return 0;
}
