/* The row cache (class_cache.c) in a toy stage: each output row is a function of the same row of three input planes and a setting; over many frames
 * with rows changed at random, a plane switched off and on, the setting changed and the size changed, the output kept by skipping the rows the cache
 * calls the same must equal the output made from scratch every frame. Also that nothing is called the same on the first frame. */
#include "viewer/class_cache.h"

#include "test.h"

#include <stdlib.h>
#include <string.h>

enum { MAX_W = 40, MAX_H = 30 };

typedef struct { int setting, third_present; } Sig;

static uint32_t seed = 12345u;
static uint32_t rnd(void) { seed = seed * 1664525u + 1013904223u; return seed >> 8; }

static void make_row(uint8_t *out, int w, const uint8_t *a, const uint8_t *b, const uint8_t *c, int setting)
{
    for (int x = 0; x < w; x++) out[x] = (uint8_t)(a[x] * 3 + b[x] * (setting + 1) + (c != NULL ? c[x] ^ 0x5a : 7));
}

static void run(int w, int h, int frames, ClassCache *cache, int *skipped, int *total)
{
    static uint8_t plane[3][MAX_W * MAX_H], kept[MAX_W * MAX_H], fresh[MAX_W * MAX_H];
    for (int p = 0; p < 3; p++)
        for (int i = 0; i < w * h; i++) plane[p][i] = (uint8_t)rnd();
    Sig sig = { 0, 1 };
    for (int f = 0; f < frames; f++) {
        const int changes = (int)(rnd() % 4);
        for (int k = 0; k < changes; k++) plane[rnd() % 3][(rnd() % (unsigned)h) * (unsigned)w + rnd() % (unsigned)w] = (uint8_t)rnd();
        if (rnd() % 11 == 0) sig.setting = (int)(rnd() % 3);
        if (rnd() % 9 == 0) sig.third_present ^= 1;
        Sig padded;
        memset(&padded, 0, sizeof padded);
        padded = sig;
        class_cache_begin(cache, &padded, sizeof padded, (size_t)w, (size_t)h, sig.third_present ? 7u : 3u);
        for (int y = 0; y < h; y++) {
            const uint8_t *in[CLASS_CACHE_INPUTS] = { plane[0], plane[1], sig.third_present ? plane[2] : NULL };   /* the planes, as class_cache.h asks */
            const int same = class_cache_row_same(cache, y, in);
            const uint8_t *c = sig.third_present ? plane[2] + y * w : NULL;
            if (!same) make_row(kept + y * w, w, plane[0] + y * w, plane[1] + y * w, c, sig.setting);
            make_row(fresh + y * w, w, plane[0] + y * w, plane[1] + y * w, c, sig.setting);
            (*total)++;
            if (same) {
                (*skipped)++;
                CHECK(f > 0);
            }
        }
        CHECK(memcmp(kept, fresh, (size_t)(w * h)) == 0);
    }
}

int main(void)
{
    ClassCache cache;
    memset(&cache, 0, sizeof cache);
    int skipped = 0, total = 0;
    run(40, 30, 400, &cache, &skipped, &total);
    run(17, 9, 400, &cache, &skipped, &total);   /* another size: the cache starts again */
    run(40, 30, 200, &cache, &skipped, &total);
    CHECK(skipped > total / 2);   /* a few changes a frame leave most rows alone */
    printf("class cache: %d of %d rows skipped\n", skipped, total);
    return TEST_REPORT();
}
