#include "game/audio/rain_bus.h"

#include <math.h>

int16_t rain_bus_sample(int16_t mix, int32_t bus)
{
    if (bus == 0) return mix;   /* no rain here: the race's own sample */
    const double sum = (double)mix + bus, a = fabs(sum), room = RAIN_BUS_FULL - RAIN_BUS_KNEE;
    if (a <= RAIN_BUS_KNEE) return (int16_t)sum;
    const double rounded = RAIN_BUS_KNEE + room * tanh((a - RAIN_BUS_KNEE) / room);
    return (int16_t)lrint(copysign(rounded < RAIN_BUS_FULL ? rounded : RAIN_BUS_FULL, sum));
}

void rain_bus_meet(RainBus *b, int16_t *mix, const int32_t *still, const int32_t *moving, uint32_t frames, uint32_t rate)
{
    const int32_t step = (int32_t)(RAIN_BUS_UNITY / (rate * RAIN_BUS_HUSH_MS / 1000 + 1)) + 1, toward = b->hushed ? RAIN_BUS_UNITY : 0;
    for (uint32_t f = 0; f < frames; f++) {
        if (b->quiet != toward) b->quiet = b->quiet < toward ? (b->quiet + step < toward ? b->quiet + step : toward) : (b->quiet - step > toward ? b->quiet - step : toward);
        for (uint32_t c = 0; c < 2; c++) {
            const uint32_t i = 2 * f + c;
            const int64_t bus = (int64_t)still[i] + (int64_t)moving[i] * RAIN_BUS_MOVING_PERMILLE / 1000;
            mix[i] = rain_bus_sample(mix[i], (int32_t)(bus * (RAIN_BUS_UNITY - b->quiet) / RAIN_BUS_UNITY));
        }
    }
}

void rain_bus_hush(RainBus *b, int32_t tick) { b->hushed = 1, b->hushed_tick = tick; }

int rain_bus_wake(RainBus *b, int32_t tick)
{
    if (!b->hushed || tick == b->hushed_tick) return 0;
    b->hushed = 0;
    return 1;
}

int rain_bus_silent(const RainBus *b) { return b->hushed && b->quiet == RAIN_BUS_UNITY; }
