/* Render bands: the job pool and the row splitter (platform_parallel_for, game/sys/bands.c). Every index runs once on
 * any number of threads, a batch's writes are visible when the call returns, and the bands cover the rows exactly
 * once, with TNFS_BANDS 0 as one plain call. */
#include "game/sys/bands.h"
#include "platform/platform.h"
#include "test.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { ROWS = 480, BATCHES = 200 };

static void bump(void *ctx, int index) { ((uint8_t *)ctx)[index]++; }

static void test_every_index_once(void)
{
    static uint8_t seen[1000];
    for (int threads = 1; threads <= 8; threads++)
        for (int count = 0; count <= 40; count += 13) {
            memset(seen, 0, sizeof seen);
            platform_parallel_for(count, bump, seen, threads);
            for (int i = 0; i < count; i++) CHECK_EQ_INT(seen[i], 1);
            for (int i = count; i < 1000; i++) CHECK_EQ_INT(seen[i], 0);
        }
}

static void test_many_batches_back_to_back(void)
{
    static uint8_t seen[64];
    memset(seen, 0, sizeof seen);
    for (int b = 0; b < BATCHES; b++) platform_parallel_for(64, bump, seen, 4);
    int wrong = 0;
    for (int i = 0; i < 64; i++) wrong += seen[i] != BATCHES;
    CHECK_EQ_INT(wrong, 0);
}

typedef struct {
    uint8_t rows[ROWS];
    int calls;
} Rows;

static void mark_rows(void *ctx, int top, int bottom)
{
    Rows *r = ctx;
    for (int y = top; y < bottom; y++) r->rows[y]++;   /* disjoint rows: no two bands write one */
}

static void count_calls(void *ctx, int top, int bottom)
{
    (void)top, (void)bottom;
    ((Rows *)ctx)->calls++;   /* only run with at most one thread */
}

static void test_bands_cover_the_rows(void)
{
    for (int threads = 0; threads <= 8; threads++) {
        bands_set_threads(threads);
        CHECK_EQ_INT(bands_threads(), threads);
        for (int range = 0; range < 4; range++) {
            static const int tops[4] = { 0, 17, 100, 479 }, bottoms[4] = { 480, 463, 101, 480 };
            static Rows r;
            memset(&r, 0, sizeof r);
            bands_rows(tops[range], bottoms[range], mark_rows, &r);
            if (threads == 0) {   /* the plain path: one call over the whole range */
                Rows once = { { 0 }, 0 };
                bands_rows(tops[range], bottoms[range], count_calls, &once);
                CHECK_EQ_INT(once.calls, 1);
            }
            for (int y = 0; y < ROWS; y++) CHECK_EQ_INT(r.rows[y], y >= tops[range] && y < bottoms[range] ? 1 : 0);
        }
    }
    bands_set_threads(-1);
}

static void test_empty_range(void)
{
    static Rows r;
    memset(&r, 0, sizeof r);
    bands_set_threads(1);
    bands_rows(50, 50, count_calls, &r);
    bands_rows(60, 40, count_calls, &r);
    CHECK_EQ_INT(r.calls, 0);
    bands_set_threads(-1);
}

/* the present's palette lookup: any number of threads gives the serial loop's words */
static void test_palette_to_xrgb(void)
{
    PlatformColor palette[256];
    for (int i = 0; i < 256; i++) palette[i] = (PlatformColor){ (uint8_t)(i * 7), (uint8_t)(i * 13 + 5), (uint8_t)(255 - i) };
    const int sizes[][2] = { { 640, 480 }, { 203, 17 }, { 3360, 1440 }, { 1, 1 }, { 1120, 480 } };
    for (int k = 0; k < 5; k++) {
        const int w = sizes[k][0], h = sizes[k][1];
        const size_t n = (size_t)w * h;
        uint8_t *px = malloc(n);
        uint32_t *want = malloc(n * 4), *got = malloc(n * 4);
        for (size_t i = 0; i < n; i++) px[i] = (uint8_t)((i * 2654435761u) >> 23);
        for (size_t i = 0; i < n; i++)
            want[i] = (uint32_t)palette[px[i]].r << 16 | (uint32_t)palette[px[i]].g << 8 | palette[px[i]].b;
        for (int threads = 0; threads <= 8; threads++) {
            memset(got, 0xa5, n * 4);
            platform_palette_to_xrgb(px, palette, got, w, h, threads);
            CHECK(memcmp(got, want, n * 4) == 0);
        }
        free(px), free(want), free(got);
    }
}

/* a locked texture's rows may be wider than the frame: the pixels go to each row's start, the padding is left alone */
static void test_palette_to_xrgb_pitch(void)
{
    PlatformColor palette[256];
    for (int i = 0; i < 256; i++) palette[i] = (PlatformColor){ (uint8_t)(i * 5), (uint8_t)(i * 11 + 3), (uint8_t)(200 - i) };
    const int w = 203, h = 97, pad = 5, stride = w + pad;
    uint8_t *px = malloc((size_t)w * h);
    uint32_t *got = malloc((size_t)stride * h * 4);
    for (int i = 0; i < w * h; i++) px[i] = (uint8_t)((i * 2654435761u) >> 23);
    for (int threads = 0; threads <= 4; threads++) {
        memset(got, 0xa5, (size_t)stride * h * 4);
        platform_palette_to_xrgb_pitch(px, palette, got, stride * 4, w, h, threads);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                const PlatformColor c = palette[px[y * w + x]];
                CHECK(got[(size_t)y * stride + x] == ((uint32_t)c.r << 16 | (uint32_t)c.g << 8 | c.b));
            }
            for (int x = w; x < stride; x++) CHECK(got[(size_t)y * stride + x] == 0xa5a5a5a5u);
        }
    }
    free(px), free(got);
}

int main(void)
{
    CHECK(platform_cpu_count() >= 1);
    test_every_index_once();
    test_many_batches_back_to_back();
    test_bands_cover_the_rows();
    test_empty_range();
    test_palette_to_xrgb();
    test_palette_to_xrgb_pitch();
    platform_shutdown();
    return TEST_REPORT();
}
