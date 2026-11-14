/* port (rain_tyre_roll): where the rain's own bus (its loops and splashes, summed in 32 bits: wet_roll_mix32, splash_mix32) meets the race's mix. A full-throttle engine already takes the
 * race's mix to full scale and a fast car's spray is over it on its own, so a plain sum clips hard. Where the rain sounds, the sum is rounded softly above RAIN_BUS_KNEE (3/4 of full
 * scale) with a tanh curve that only reaches full scale at infinity; below the knee, and wherever the bus is silent, the sum is the plain one (the race's mix untouched).
 * The bus has two inputs: the rain round a stopped car as it is, and the moving rain (the loops round a moving car, every car's spray, the splashes) at RAIN_BUS_MOVING_PERMILLE.
 * The pause and the in-race menu hush it: it falls silent in RAIN_BUS_HUSH_MS, as the engine's voices go quiet there, and wakes when the race's clock moves again. */
#ifndef TNFS_RAIN_BUS_H
#define TNFS_RAIN_BUS_H

#include <stdint.h>

enum {
    RAIN_BUS_FULL = 32767, RAIN_BUS_KNEE = 24576,
    RAIN_BUS_MOVING_PERMILLE = 500,   /* the moving rain at half (-6 dB) */
    RAIN_BUS_HUSH_MS = 20,            /* full to silent, and back */
    RAIN_BUS_UNITY = 65536
};

typedef struct {
    int32_t quiet;        /* 0 heard whole .. RAIN_BUS_UNITY silent, eased toward the hush */
    int hushed;
    int32_t hushed_tick;  /* the race's tick it was hushed at */
} RainBus;   /* all 0: heard, not hushed */

/* One sample of the race's mix `mix` with the bus' `bus` over it */
int16_t rain_bus_sample(int16_t mix, int32_t bus);

/* `frames` stereo frames of the race's mix (interleaved) with the bus' two inputs over them (`still`, `moving`), in place, at `rate` frames a second */
void rain_bus_meet(RainBus *b, int16_t *mix, const int32_t *still, const int32_t *moving, uint32_t frames, uint32_t rate);

/* The race stands still at `tick` (the pause, the in-race menu): the bus falls silent */
void rain_bus_hush(RainBus *b, int32_t tick);
/* The race's tick now: hushed and the clock moved, the bus wakes (1, the caller drops what its voices held); 0 otherwise */
int rain_bus_wake(RainBus *b, int32_t tick);
/* 1 once hushed and silent: the voices can be dropped */
int rain_bus_silent(const RainBus *b);

#endif
