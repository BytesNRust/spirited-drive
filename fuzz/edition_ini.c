/* editions/<id>.ini: a Restomod edition (dep59 step 4). A file that reads writes back to one that reads the same */
#include "fuzz/fuzz.h"
#include "formats/edition_ini.h"

#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static Edition e, back;
    static char text[EDITION_MAX_SIZE];
    char why[160];
    if (!edition_parse(data, size, &e, why, sizeof why)) return 0;
    const int n = edition_format(&e, text, sizeof text);
    if (n <= 0 || !edition_parse((const uint8_t *)text, (size_t)n, &back, why, sizeof why) || memcmp(&e, &back, sizeof e) != 0) abort();
    fuzz_touch((const uint8_t *)e.name, strlen(e.name));
    return 0;
}
