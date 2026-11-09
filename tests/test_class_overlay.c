/* The 2D layer's marks over the world mask, a row at a time (class_overlay.c), against the per-pixel loop it replaced (kept here as the reference):
 * on random layers at output scales 1, 2 and 3 the masks must be byte for byte the same, whatever the mask held before and whichever classes the layer
 * holds (all 256 values); then a timing of both on a 16:9 render-scale-2 sized layer (printed, not asserted). */
#include "viewer/class_overlay.h"

#include "viewer/drive_light.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* drive_light.c's loop before class_overlay.c */
static void reference(uint8_t *world_mask, const uint8_t *class_layer, int class_width, int class_scale, int rect_width, size_t count)
{
    const size_t rows = count / (size_t)rect_width;
    for (size_t y = 0; y < rows; y++) {
        const uint8_t *row = class_layer + (y / (size_t)class_scale) * (size_t)class_width;
        for (size_t x = 0; x < (size_t)rect_width; x++) {
            const uint8_t c = row[x / (size_t)class_scale];
            if (c == PSX_CLASS_DASH) world_mask[y * (size_t)rect_width + x] = 2;
            else if (c == PSX_CLASS_HUD) world_mask[y * (size_t)rect_width + x] = 0;
            else if (c >= PSX_CLASS_LIT && c < PSX_CLASS_LIT_COLOUR + PSX_CLASS_LIT_COLOURS) world_mask[y * (size_t)rect_width + x] = c;
        }
    }
}

static uint32_t seed = 0x2545f491u;
static uint32_t rnd(void) { seed = seed * 1664525u + 1013904223u; return seed >> 8; }

static void fill_layer(uint8_t *layer, size_t n, int density)
{
    if (density < 0) {   /* runs of one class, as the HUD's bars and the bars of the picture are: lengths from 1 to a few hundred */
        for (size_t i = 0; i < n;) {
            const uint8_t c = rnd() % 3 == 0 ? (uint8_t)(rnd() % 20) : rnd() % 2 ? 0xff : (uint8_t)(1 + rnd() % 15);
            size_t len = 1 + (rnd() % 4 == 0 ? rnd() % 400 : rnd() % 12);
            for (; len > 0 && i < n; len--) layer[i++] = c;
        }
        return;
    }
    for (size_t i = 0; i < n; i++) layer[i] = rnd() % 100 < (uint32_t)density ? (uint8_t)(rnd() % 256) : 0xff;   /* mostly "nothing drawn" */
}

static void compare(int width, int height, int scale, int density, int rounds)
{
    const size_t n = (size_t)width * (size_t)height, out = n * (size_t)scale * (size_t)scale;
    uint8_t *layer = malloc(n), *marks = malloc((size_t)height), *a = malloc(out), *b = malloc(out);
    int bad = 0;
    for (int r = 0; r < rounds; r++) {
        fill_layer(layer, n, density);
        for (size_t i = 0; i < out; i++) a[i] = b[i] = (uint8_t)rnd();   /* what the diff left in the mask */
        reference(a, layer, width, scale, width * scale, out);
        class_overlay_rows(layer, width, height, marks);
        class_overlay_apply(b, width * scale, out / (size_t)(width * scale), layer, width, height, scale, marks);
        if (memcmp(a, b, out) != 0) bad++;
    }
    CHECK(bad == 0);
    free(layer), free(marks), free(a), free(b);
}

static double seconds(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec + (double)t.tv_nsec * 1e-9; }

static void timing(void)
{
    enum { W = 853, H = 480, S = 2, FRAMES = 20 };
    const size_t n = (size_t)W * H, out = n * S * S;
    uint8_t *layer = malloc(n), *marks = malloc(H), *mask = malloc(out);
    memset(layer, 0xff, n);
    for (int y = 0; y < H; y++) {   /* a chase view's layer: the bars at the sides and the bottom are HUD (64% of the layer), the 3D shows between */
        memset(layer + (size_t)y * W, PSX_CLASS_HUD, 150);
        memset(layer + (size_t)y * W + W - 150, PSX_CLASS_HUD, 150);
    }
    for (int y = 400; y < H; y++) memset(layer + (size_t)y * W, PSX_CLASS_HUD, W);
    memset(mask, 1, out);
    double t0 = seconds();
    for (int f = 0; f < FRAMES; f++) reference(mask, layer, W, S, W * S, out);
    const double old = (seconds() - t0) / FRAMES;
    t0 = seconds();
    for (int f = 0; f < FRAMES; f++) {
        class_overlay_rows(layer, W, H, marks);
        class_overlay_apply(mask, W * S, out / (size_t)(W * S), layer, W, H, S, marks);
    }
    const double now = (seconds() - t0) / FRAMES;
    printf("class overlay %dx%d at scale %d: per-pixel loop %.3f ms, by rows %.3f ms\n", W, H, S, old * 1e3, now * 1e3);
    free(layer), free(marks), free(mask);
}

int main(void)
{
    for (int scale = 1; scale <= 3; scale++) {
        compare(64, 48, scale, 0, 4);      /* nothing drawn */
        compare(64, 48, scale, 3, 40);     /* a few marks */
        compare(64, 48, scale, 50, 40);
        compare(64, 48, scale, 100, 40);   /* every class value */
        compare(64, 48, scale, -1, 80);    /* runs */
        compare(643, 31, scale, -1, 40);   /* runs across the eight-byte steps, a width that is no multiple of 8 */
        compare(1, 1, scale, 100, 10);     /* one pixel */
        compare(7, 5, scale, 40, 40);      /* odd sizes */
    }
    timing();
    return TEST_REPORT();
}
