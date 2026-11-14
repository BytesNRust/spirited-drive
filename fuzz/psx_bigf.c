/* PSX BIGF archives and their SHPP images: the PlayStation liveries and skies a player points the port at */
#include "fuzz/fuzz.h"
#include "formats/psx_bigf.h"

#include <stdlib.h>

static void image(const uint8_t *file, size_t size, const char *name)
{
    PsxImage8 im;
    if (!psx_shpp_image8(file, size, name, &im)) return;
    fuzz_touch(im.pixels, (size_t)im.width * (size_t)im.height);
    if (im.clut != NULL) for (int i = 0; i < 256; i++) { uint8_t rgb[3]; psx_clut_rgb(im.clut, i, rgb); }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static const char *const MEMBERS[] = { "a.psh", "car.psh", "sky.psh" };
    uint32_t *counts = calloc(PSX_COLOURS, sizeof *counts);
    if (counts == NULL) return 0;
    for (size_t m = 0; m < sizeof MEMBERS / sizeof MEMBERS[0]; m++) {
        const uint8_t *member = NULL;
        size_t length = 0;
        if (!psx_bigf_member(data, size, MEMBERS[m], &member, &length)) continue;
        fuzz_touch(member, length);
        (void)psx_shpp_colours(member, length, counts);
        image(member, length, "hrz0");
    }
    (void)psx_shpp_count(data, size);
    (void)psx_shpp_colours(data, size, counts);
    image(data, size, "hrz0");
    image(data, size, "shad");
    free(counts);
    return 0;
}
