#include "formats/fixed_record.h"

#include "formats/qfs.h"

#include <stdlib.h>
#include <string.h>

int fixed_record_read(uint8_t *out, size_t record_size, const uint8_t *file, size_t size)
{
    size_t unpacked = 0;
    if (!qfs_is_compressed(file, size)) {
        if (size < record_size) return 0;
        memcpy(out, file, record_size);
        return 1;
    }
    if (qfs_decompressed_size(file, size, &unpacked) != QFS_OK || unpacked < record_size) return 0;
    uint8_t *data = malloc(unpacked);
    const int ok = data != NULL && qfs_decompress(file, size, data, unpacked) == QFS_OK;
    if (ok) memcpy(out, data, record_size);
    free(data);
    return ok;
}
