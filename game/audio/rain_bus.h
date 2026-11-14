/* port (rain_tyre_roll): where the rain's own bus (its loops and splashes, summed in 32 bits: wet_roll_mix32, splash_mix32) meets the race's mix. A full-throttle engine already takes the
 * race's mix to full scale and a fast car's spray is over it on its own, so a plain sum clips hard. Where the rain sounds, the sum is rounded softly above RAIN_BUS_KNEE (3/4 of full
 * scale) with a tanh curve that only reaches full scale at infinity; below the knee, and wherever the bus is silent, the sum is the plain one (the race's mix untouched).
 * The bus has two inputs: the rain round a stopped car as it is, and the moving rain (the loops round a moving car, every car's spray, the splashes) at RAIN_BUS_MOVING_PERMILLE.
 * Where it meets the mix it takes the weather's hush (hush.h): the pause and the in-race menu silence it. */
#ifndef TNFS_RAIN_BUS_H
#define TNFS_RAIN_BUS_H

#include <stdint.h>

enum {
    RAIN_BUS_FULL = 32767, RAIN_BUS_KNEE = 24576,
    RAIN_BUS_MOVING_PERMILLE = 500   /* the moving rain at half (-6 dB) */
};

/* One sample of the race's mix `mix` with the bus' `bus` over it */
int16_t rain_bus_sample(int16_t mix, int32_t bus);

/* `frames` stereo frames of the race's mix (interleaved) with the bus' two inputs over them (`still`, `moving`) at each frame's `gain` (0..HUSH_UNITY, hush.h), in place */
void rain_bus_meet(int16_t *mix, const int32_t *still, const int32_t *moving, const int32_t *gain, uint32_t frames);

#endif
