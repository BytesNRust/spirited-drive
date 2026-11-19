#include "art.h"

#include <SDL3/SDL.h>

#include <stdio.h>

static const char NAME[] = "GENERIC.FAM";

/* 1 when `path` is a file */
static int is_file(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

int art_generic_find(char *out, size_t cap)
{
    static const char *const BESIDE[] = { "", "../Resources/" };   /* the folder SDL names, then the bundle's Resources from MacOS */
    const char *base = SDL_GetBasePath();   /* ".../Spirited Drive Studio.app/Contents/Resources/" in a bundle, the program's folder otherwise */
    if (base == NULL) return 0;
    for (size_t i = 0; i < sizeof BESIDE / sizeof *BESIDE; ++i) {
        char path[4096];
        const int n = snprintf(path, sizeof path, "%s%s%s", base, BESIDE[i], NAME);
        if (n > 0 && (size_t)n < sizeof path && (size_t)n < cap && is_file(path)) {
            snprintf(out, cap, "%s", path);
            return 1;
        }
    }
    return 0;
}
