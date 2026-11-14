/* ICO: the original's window icon */
#include "fuzz/fuzz.h"
#include "formats/ico.h"

#include <stdlib.h>

enum { CAPACITY = 256 * 256 * 4 };

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint8_t *rgba = malloc(CAPACITY);
    if (rgba == NULL) return 0;
    int w = 0, h = 0;
    (void)ico_decode(data, size, rgba, CAPACITY, &w, &h);
    free(rgba);
    return 0;
}
