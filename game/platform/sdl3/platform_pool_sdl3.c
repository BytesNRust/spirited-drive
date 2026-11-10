/* The SDL3 platform's clock and job pool (platform_sdl3.c split by responsibility; the shared names are in platform_sdl3_private.h). */
#include "platform/platform.h"

#include "formats/modpath.h"
#include "formats/validate.h"
#include "game/sys/key_latch.h"
#include "game/sys/data_path.h"
#include "game/sys/file_url.h"
#include "game/sys/port_log.h"
#include "tnfs_version.h"
#include "platform/sdl3/input_script.h"
#include "platform/sdl3/input_shared.h"
#include "platform/sdl3/pad_keys.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <unistd.h>   /* fsync */
#include <limits.h>   /* realpath: symbolic links out of the mods directory are refused */
#define TNFS_HAVE_REALPATH 1
#endif
#include "platform/sdl3/platform_sdl3_private.h"

uint32_t platform_ticks_ms(void)
{
    return (uint32_t)SDL_GetTicks();
}

void platform_sleep_ms(uint32_t ms)
{
#ifdef __EMSCRIPTEN__
    /* With ASYNCIFY this unwinds to the browser so the tab stays responsive. */
    emscripten_sleep(ms);
#else
    SDL_Delay(ms);
#endif
}

/* The job pool (platform.h). Workers are made the first time a batch asks for them and wait on `wake`; a batch is
 * published, `needed` workers woken, and the caller claims indexes with them. Every woken worker checks in on `parked`
 * once it can claim no more, and the caller waits for all of them before it returns, so no worker is ever inside a
 * batch while the next one is set up. */
#ifndef __EMSCRIPTEN__
enum { POOL_MAX = 15, POOL_STACK_BYTES = 1 << 20 };
static struct {
    SDL_Thread *thread[POOL_MAX];
    int workers;
    SDL_Semaphore *wake, *parked;
    void (*job)(void *ctx, int index);
    void *ctx;
    int count;
    SDL_AtomicInt next, quit;
} pool;

static void pool_run(void)
{
    for (;;) {
        const int i = SDL_AddAtomicInt(&pool.next, 1);
        if (i >= pool.count) return;
        pool.job(pool.ctx, i);
    }
}

static int pool_worker(void *unused)
{
    (void)unused;
    for (;;) {
        SDL_WaitSemaphore(pool.wake);
        if (SDL_GetAtomicInt(&pool.quit) != 0) return 0;
        pool_run();
        SDL_SignalSemaphore(pool.parked);
    }
}

static int pool_grow(int workers)
{
    if (workers > POOL_MAX) workers = POOL_MAX;
    if (pool.wake == NULL) {
        pool.wake = SDL_CreateSemaphore(0);
        pool.parked = SDL_CreateSemaphore(0);
        if (pool.wake == NULL || pool.parked == NULL) return 0;
    }
    while (pool.workers < workers) {
        /* port: a stack of its own size: the rasterizers keep their edge tables on it (quad.c's about 34 KB at 1440 rows) */
        const SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void *)pool_worker);
        SDL_SetStringProperty(props, SDL_PROP_THREAD_CREATE_NAME_STRING, "tnfs-job");
        SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, POOL_STACK_BYTES);
        pool.thread[pool.workers] = SDL_CreateThreadWithProperties(props);
        SDL_DestroyProperties(props);
        if (pool.thread[pool.workers] == NULL) break;
        pool.workers++;
    }
    return pool.workers;
}

void pool_stop(void)
{
    if (pool.workers == 0) return;
    SDL_SetAtomicInt(&pool.quit, 1);
    for (int i = 0; i < pool.workers; i++) SDL_SignalSemaphore(pool.wake);
    for (int i = 0; i < pool.workers; i++) SDL_WaitThread(pool.thread[i], NULL);
    pool.workers = 0;
    SDL_SetAtomicInt(&pool.quit, 0);
}
#endif

int platform_cpu_count(void)
{
#ifdef __EMSCRIPTEN__
    return 1;
#else
    const int n = SDL_GetNumLogicalCPUCores();
    return n < 1 ? 1 : n;
#endif
}

void platform_parallel_for(int count, void (*job)(void *ctx, int index), void *ctx, int threads)
{
#ifndef __EMSCRIPTEN__
    if (threads > 1 && count > 1) {
        const int wanted = (threads < count ? threads : count) - 1;
        const int workers = pool_grow(wanted);
        const int used = workers < wanted ? workers : wanted;
        if (used > 0) {
            pool.job = job, pool.ctx = ctx, pool.count = count;
            SDL_SetAtomicInt(&pool.next, 0);
            for (int i = 0; i < used; i++) SDL_SignalSemaphore(pool.wake);
            pool_run();
            for (int i = 0; i < used; i++) SDL_WaitSemaphore(pool.parked);
            return;
        }
    }
#else
    (void)threads;
#endif
    for (int i = 0; i < count; i++) job(ctx, i);
}
