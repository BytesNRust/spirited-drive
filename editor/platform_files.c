/* The two platform functions the game's livery and mask code reads files through (game/platform/platform.h), for the Studio, which links that code without the game's SDL
 * platform: a whole file from stdio. */
#include "game/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>

uint8_t *platform_read_file(const char *path, size_t *size)
{
    *size = 0;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *buf = NULL;
    size_t n = 0, cap = 0;
    for (;;) {
        if (n == cap) {
            cap = cap ? cap * 2 : 1 << 16;
            uint8_t *bigger = realloc(buf, cap);
            if (bigger == NULL) { free(buf); fclose(f); return NULL; }
            buf = bigger;
        }
        const size_t got = fread(buf + n, 1, cap - n, f);
        n += got;
        if (got == 0) break;
    }
    fclose(f);
    if (buf == NULL) buf = malloc(1);
    *size = n;
    return buf;
}

void platform_free_file(uint8_t *data)
{
    free(data);
}
