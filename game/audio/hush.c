#include "game/audio/hush.h"

void hush_at(Hush *h, int32_t tick) { h->hushed = 1, h->tick = tick; }

int hush_wake(Hush *h, int32_t tick)
{
    if (!h->hushed || tick == h->tick) return 0;
    h->hushed = 0;
    return 1;
}

int hush_silent(const Hush *h) { return h->hushed && h->quiet == HUSH_UNITY; }

void hush_gains(Hush *h, int32_t *gain, uint32_t frames, uint32_t rate)
{
    const int32_t step = (int32_t)(HUSH_UNITY / (rate * HUSH_MS / 1000 + 1)) + 1, toward = h->hushed ? HUSH_UNITY : 0;
    for (uint32_t f = 0; f < frames; f++) {
        if (h->quiet != toward) h->quiet = h->quiet < toward ? (h->quiet + step < toward ? h->quiet + step : toward) : (h->quiet - step > toward ? h->quiet - step : toward);
        gain[f] = HUSH_UNITY - h->quiet;
    }
}
