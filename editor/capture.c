#include "capture.h"

#include "formats/png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int save_rgba_png(SDL_Surface *surface, const char *path, char *why, size_t why_cap)
{
    const uint32_t w = (uint32_t)surface->w, h = (uint32_t)surface->h;
    uint8_t *tight = malloc((size_t)w * h * 4);
    if (tight == NULL) {
        snprintf(why, why_cap, "out of memory");
        return 0;
    }
    for (uint32_t row = 0; row < h; ++row)
        memcpy(tight + (size_t)row * w * 4, (const uint8_t *)surface->pixels + (size_t)row * (size_t)surface->pitch, (size_t)w * 4);
    uint8_t *png = NULL;
    size_t len = 0;
    const int encoded = png_write_rgba_fast(tight, w, h, &png, &len) == PNG_OK;
    free(tight);
    if (!encoded) {
        snprintf(why, why_cap, "the picture does not encode");
        return 0;
    }
    FILE *f = fopen(path, "wb");
    const int ok = f != NULL && fwrite(png, 1, len, f) == len;
    if (f != NULL && fclose(f) != 0) snprintf(why, why_cap, "%s: cannot close it", path);
    png_free(png);
    if (!ok) snprintf(why, why_cap, "%s: cannot write it", path);
    return ok;
}

int capture_save_png(SDL_Renderer *ren, const char *path, char *why, size_t why_cap)
{
    SDL_Surface *read = SDL_RenderReadPixels(ren, NULL);
    if (read == NULL) {
        snprintf(why, why_cap, "cannot read the window: %s", SDL_GetError());
        return 0;
    }
    SDL_Surface *rgba = SDL_ConvertSurface(read, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(read);
    if (rgba == NULL) {
        snprintf(why, why_cap, "cannot convert the window: %s", SDL_GetError());
        return 0;
    }
    const int ok = save_rgba_png(rgba, path, why, why_cap);
    SDL_DestroySurface(rgba);
    return ok;
}
