/* A title screen's art (the CD's or a mod's unpacked screen): its bgnd and palette */
#include "fuzz/fuzz.h"
#include "game/frontend/title_art.h"

#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint8_t *copy = malloc(size ? size : 1);   /* the reader works on the file's own bytes */
    if (copy == NULL) return 0;
    if (size) memcpy(copy, data, size);
    uint8_t palette[768];
    TitleArtScreen s;
    if (title_art_screen(copy, size, palette, &s)) fuzz_touch(s.pixels, 640u * 480u);
    free(copy);
    return 0;
}
