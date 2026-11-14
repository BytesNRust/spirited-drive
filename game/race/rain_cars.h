/* port (rain_tyre_roll): the cars' own sounds in the rain, beside the layers of rain_roll.h: each car's tyre spray is a source where the car is, and its tyres throw puddle splashes at
 * random, more the faster it goes. The voices are few (RAIN_CARS_VOICES): the loudest cars have them, a car that loses one lets it fall silent before another car takes it.
 * Pure: the dice are the caller's (no race_rand: the race's own random numbers stay the exe's), and so is the state. */
#ifndef TNFS_RAIN_CARS_H
#define TNFS_RAIN_CARS_H

#include <stdint.h>

enum {
    RAIN_CARS_VOICES = 4,                  /* the sprays heard at once (the loudest cars) */
    RAIN_CARS_REST_TICKS = 12,             /* race ticks a voice rests after its car lost it, so its fade (a tenth of a second) ends before another car takes it */
    RAIN_PUDDLE_FROM = 0xa0000,            /* 10 m/s (16.16): slower, no splash */
    RAIN_PUDDLE_TOP_MILLIHERTZ = 1000,     /* the splashes a second (x 1000) at RAIN_ROLL_FULL and beyond, rising straight from RAIN_PUDDLE_FROM */
    RAIN_PUDDLE_PERCENT = 100,             /* a splash's level near the car, before its random gain (the samples peak at -4 dBFS) */
    RAIN_PUDDLE_GAIN_LOW = 501,            /* the random gain, permille: -6 dB ... */
    RAIN_PUDDLE_GAIN_HIGH = 1000,          /* ... to 0 dB, even in dB */
    RAIN_PUDDLE_PITCH_SPREAD = 50,         /* the random rate, permille of the sample's own: 1000 +- this (5 %) */
    RAIN_PUDDLE_MAX_CLIPS = 8              /* rain_puddle_1.wav .. rain_puddle_8.wav */
};

/* the next of a xorshift32 sequence (`*seed` never 0: 0 is taken as 1) */
uint32_t rain_dice(uint32_t *seed);

/* The splashes a second (x 1000) of a car at `speed` (16.16 m/s, either sign). */
int rain_puddle_millihertz(int32_t speed);

typedef struct {
    int clip;           /* which sample, 0 .. clips - 1 */
    int gain_permille;  /* RAIN_PUDDLE_GAIN_LOW .. RAIN_PUDDLE_GAIN_HIGH */
    int pitch_permille; /* 1000 +- RAIN_PUDDLE_PITCH_SPREAD */
} RainPuddle;

/* Whether a car at `speed` threw a splash over `ticks` race ticks (RACE_TICKS_A_SECOND a second; at most one): 1 and `out` filled (a clip of `clips`, not `last` when there are two
 * or more), else 0. Every call draws from `seed` the same number of times. */
int rain_puddle_roll(uint32_t *seed, int32_t speed, uint32_t ticks, int clips, int last, RainPuddle *out);

typedef struct {
    int car[RAIN_CARS_VOICES];           /* the car (its slot) each voice is for, -1 none */
    int fresh[RAIN_CARS_VOICES];         /* 1: the voice was taken by this call (its loop starts somewhere new) */
    int32_t free_at[RAIN_CARS_VOICES];   /* the tick its car lost it */
    int32_t last_tick;                   /* the tick of the last call */
} RainCarVoices;

void rain_car_voices_clear(RainCarVoices *v);

/* The voices for this frame: `wanted` the cars heard (their slots), the loudest first, `n` of them. A car keeps its voice while it is among the RAIN_CARS_VOICES + 1 loudest; a car
 * among the RAIN_CARS_VOICES loudest without one takes a voice that has rested RAIN_CARS_REST_TICKS since its car lost it. `tick` the race's: one gone back (a new race, a replay)
 * frees all at once. */
void rain_car_voices_assign(RainCarVoices *v, const int *wanted, int n, int32_t tick);

#endif
