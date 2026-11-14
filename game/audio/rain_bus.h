/* port (rain_tyre_roll): where the rain's own bus (its loops and splashes, summed in 32 bits: wet_roll_mix32, splash_mix32) meets the race's mix. A full-throttle engine already takes the
 * race's mix to full scale and a fast car's spray is over it on its own, so a plain sum clips hard. Where the rain sounds, the sum is rounded softly above RAIN_BUS_KNEE (3/4 of full
 * scale) with a tanh curve that only reaches full scale at infinity; below the knee, and wherever the bus is silent, the sum is the plain one (the race's mix untouched). */
#ifndef TNFS_RAIN_BUS_H
#define TNFS_RAIN_BUS_H

#include <stdint.h>

enum { RAIN_BUS_FULL = 32767, RAIN_BUS_KNEE = 24576 };

/* One sample of the race's mix `mix` with the bus' `bus` over it */
int16_t rain_bus_sample(int16_t mix, int32_t bus);

/* `frames` stereo frames of the race's mix (interleaved) with the bus over them, in place */
void rain_bus_meet(int16_t *mix, const int32_t *bus, uint32_t frames);

#endif
