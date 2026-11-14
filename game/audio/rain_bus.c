#include "game/audio/rain_bus.h"

#include "game/audio/hush.h"

#include <math.h>

int16_t rain_bus_sample(int16_t mix, int32_t bus)
{
    if (bus == 0) return mix;   /* no rain here: the race's own sample */
    const double sum = (double)mix + bus, a = fabs(sum), room = RAIN_BUS_FULL - RAIN_BUS_KNEE;
    if (a <= RAIN_BUS_KNEE) return (int16_t)sum;
    const double rounded = RAIN_BUS_KNEE + room * tanh((a - RAIN_BUS_KNEE) / room);
    return (int16_t)lrint(copysign(rounded < RAIN_BUS_FULL ? rounded : RAIN_BUS_FULL, sum));
}

void rain_bus_meet(int16_t *mix, const int32_t *still, const int32_t *moving, const int32_t *gain, uint32_t frames)
{
    for (uint32_t f = 0; f < frames; f++) {
        for (uint32_t c = 0; c < 2; c++) {
            const uint32_t i = 2 * f + c;
            const int64_t bus = (int64_t)still[i] + (int64_t)moving[i] * RAIN_BUS_MOVING_PERMILLE / 1000;
            mix[i] = rain_bus_sample(mix[i], (int32_t)(bus * gain[f] / HUSH_UNITY));
        }
    }
}
