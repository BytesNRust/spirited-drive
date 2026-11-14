/* FSH / SHPI: the art archives (title, menus, car and track bitmaps) */
#include "fuzz/fuzz.h"
#include "formats/fsh.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Fsh fsh;
    if (fsh_open(&fsh, data, size) != FSH_OK) return 0;
    for (uint32_t i = 0; i < fsh.count; i++) {
        FshEntry e;
        if (fsh_entry(&fsh, i, &e) != FSH_OK) continue;
        fuzz_touch(e.payload, e.payload_size);
        uint8_t rgb[768];
        (void)fsh_palette_rgb(&e, rgb);
        uint32_t found;
        (void)fsh_find(&fsh, e.name, &found);
    }
    uint32_t found;
    (void)fsh_find(&fsh, "!pal", &found);
    return 0;
}
