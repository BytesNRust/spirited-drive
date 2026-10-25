#include "game/sys/bands.h"

#include "platform/platform.h"

#include <stdlib.h>
#include <string.h>

enum { MAX_THREADS = 8, BANDS_A_THREAD = 4, MIN_ROWS = 8 };

static int threads = -1;   /* -1: not read yet */

static int from_environment(void)
{
    const char *v = getenv("TNFS_BANDS");
    if (v == NULL || *v == '\0') return 0;
    if (strcmp(v, "auto") == 0) {
        const int n = platform_cpu_count();
        return n > MAX_THREADS ? MAX_THREADS : n;
    }
    const int n = atoi(v);
    return n < 0 ? 0 : n > MAX_THREADS ? MAX_THREADS : n;
}

int bands_threads(void)
{
    if (threads < 0) {
        threads = from_environment();
        platform_set_present_threads(threads);
    }
    return threads;
}

void bands_set_threads(int n)
{
    threads = n < 0 ? -1 : n > MAX_THREADS ? MAX_THREADS : n;
    platform_set_present_threads(threads < 0 ? 0 : threads);
}

typedef struct {
    int top, bottom, count;
    void (*fn)(void *ctx, int top, int bottom);
    void *ctx;
} Split;

static void run_band(void *user, int index)
{
    const Split *s = user;
    bands_set_index(index);
    const int rows = s->bottom - s->top;
    const int from = s->top + (int)((int64_t)rows * index / s->count);
    const int to = s->top + (int)((int64_t)rows * (index + 1) / s->count);
    if (to > from) s->fn(s->ctx, from, to);
}

void bands_rows(int top, int bottom, void (*fn)(void *ctx, int top, int bottom), void *ctx)
{
    const int n = bands_threads();
    if (bottom <= top) return;
    if (n == 0) {
        bands_set_index(0);
        fn(ctx, top, bottom);
        return;
    }
    int count = n * BANDS_A_THREAD;
    if (count > (bottom - top) / MIN_ROWS) count = (bottom - top) / MIN_ROWS;
    if (count < 1) count = 1;
    const Split s = { top, bottom, count, fn, ctx };
    platform_parallel_for(count, run_band, (void *)&s, n);
}
