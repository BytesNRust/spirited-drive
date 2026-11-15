/* mod.ini: a mod's card (dep59) */
#include "fuzz/fuzz.h"
#include "formats/mod_ini.h"

#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ModInfo info;
    char why[160];
    if (mod_ini_parse(data, size, &info, why, sizeof why)) fuzz_touch((const uint8_t *)info.id, strlen(info.id));
    return 0;
}
