/* A row-by-row result kept between frames (class_cache.h). */
#include "viewer/class_cache.h"

#include <stdlib.h>
#include <string.h>

void class_cache_begin(ClassCache *cache, const void *signature, size_t signature_size, size_t width, size_t height, unsigned used)
{
    int same = cache->ready && cache->width == width && cache->height == height && cache->signature_size == signature_size &&
               cache->signature != NULL && memcmp(cache->signature, signature, signature_size) == 0;
    if (cache->width != width || cache->height != height) {
        for (int i = 0; i < CLASS_CACHE_INPUTS; i++) {
            free(cache->copy[i]);
            cache->copy[i] = NULL;
        }
        cache->width = width, cache->height = height;
        same = 0;
    }
    if (cache->signature_size != signature_size) {
        uint8_t *s = realloc(cache->signature, signature_size);
        if (s == NULL) {
            cache->ready = cache->reuse = 0;
            cache->signature_size = 0;
            return;
        }
        cache->signature = s, cache->signature_size = signature_size;
        same = 0;
    }
    for (int i = 0; i < CLASS_CACHE_INPUTS; i++) {
        if ((used >> i & 1u) == 0 || cache->copy[i] != NULL) continue;
        cache->copy[i] = calloc(width * height, 1);   /* the first frame of a size (or of this plane): nothing to be the same as */
        same = 0;
        if (cache->copy[i] == NULL) {
            cache->ready = cache->reuse = 0;
            return;
        }
    }
    if (!same) memcpy(cache->signature, signature, signature_size);
    cache->ready = 1;
    cache->reuse = same;
}

int class_cache_row_same(ClassCache *cache, int y, const uint8_t *const input[CLASS_CACHE_INPUTS])
{
    int same = cache->reuse;
    for (int i = 0; i < CLASS_CACHE_INPUTS; i++) {
        if (input[i] == NULL) continue;
        if (cache->copy[i] == NULL) return 0;   /* not asked for in class_cache_begin (a caller's mistake): never the same */
        uint8_t *kept = cache->copy[i] + (size_t)y * cache->width;
        const uint8_t *row = input[i] + (size_t)y * cache->width;
        if (memcmp(kept, row, cache->width) != 0) {
            memcpy(kept, row, cache->width);
            same = 0;
        }
    }
    return same;
}
