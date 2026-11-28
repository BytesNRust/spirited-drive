/* See perf.h. */
#include "perf.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static int on = -1;
static uint64_t total[PERF_SLOTS], worst[PERF_SLOTS], started;
static int frames;

static const char *const NAME[PERF_SLOTS] = { "dash", "poll", "watch", "recompute", "model", "masks", "preview", "ui", "present", "pause" };

int perf_on(void)
{
    if (on < 0) on = getenv("TNFS_PERF") != NULL && getenv("TNFS_PERF")[0] != '\0';
    return on;
}

uint64_t perf_begin(void)
{
    return perf_on() ? SDL_GetPerformanceCounter() : 0;
}

void perf_end(PerfSlot slot, uint64_t t0)
{
    if (t0 == 0) return;
    const uint64_t dt = SDL_GetPerformanceCounter() - t0;
    total[slot] += dt;
    if (dt > worst[slot]) worst[slot] = dt;
}

void perf_frame(void)
{
    if (!perf_on()) return;
    const uint64_t now = SDL_GetPerformanceCounter(), freq = SDL_GetPerformanceFrequency();
    ++frames;
    if (started == 0) started = now;
    if (now - started < freq) return;
    const double ms = 1000.0 / (double)freq, span = (double)(now - started) * ms;
    fprintf(stderr, "perf: %d frames in %.0f ms |", frames, span);
    for (int i = 0; i < PERF_SLOTS; ++i) {
        fprintf(stderr, " %s %.1f (max %.1f)", NAME[i], (double)total[i] * ms, (double)worst[i] * ms);
        total[i] = worst[i] = 0;
    }
    fprintf(stderr, " ms\n");
    frames = 0, started = now;
}
