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

void rain_bus_meet(int16_t *mix, const int32_t *bus, uint32_t frames)
{
    for (uint32_t i = 0; i < 2 * frames; i++) mix[i] = rain_bus_sample(mix[i], bus[i]);
}
