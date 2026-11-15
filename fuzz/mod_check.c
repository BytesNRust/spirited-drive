/* the install check of a mod's zip (dep59): names, kinds and every file's content through the game's readers */
#include "fuzz/fuzz.h"
#include "formats/mod_check.h"

#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ModCheck c;
    char why[256];
    if (mod_check_zip(data, size, &c, why, sizeof why)) fuzz_touch((const uint8_t *)c.info.id, strlen(c.info.id));
    return 0;
}
