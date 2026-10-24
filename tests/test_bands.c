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

int main(void)
{
    CHECK(platform_cpu_count() >= 1);
    test_every_index_once();
    test_many_batches_back_to_back();
    test_bands_cover_the_rows();
    test_empty_range();
    platform_shutdown();
    return TEST_REPORT();
}
