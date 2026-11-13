/* port (rain_tyre_roll): what the followed car hears of the rain (rain_v2), three looping layers mixed over the race's own (the audio side is game/audio/wet_roll.h, one voice each):
 * - stopped, outside: the rain falling round the car (data/sounds/rain_stopped_out.wav);
 * - stopped, in the cockpit: the rain on the roof (data/sounds/rain_stopped_in.wav);
 * - moving: the wet road under the tyres (data/sounds/rain_tyre_roll.wav), on asphalt; on the rally's gravel RAIN_ROLL_GRAVEL_PERCENT of it, a layer under the exe's gravel roll (wet gravel, mud);
 *   none on grass, a shoulder or in the air.
 * Falling snow is silent (no stopped layer) but melts on the road: the moving layer is the rain's. The stopped layer gives way to the moving one with an equal-power cross-fade from RAIN_ROLL_MOVING
 * to RAIN_ROLL_CROSSED; the moving one then rises to RAIN_ROLL_FULL. The cockpit hears the moving layer 6 dB lower. The openness scales all three (0 inside a closed zone, fading at the mouths).
 * Pure functions of what falls, the openness, the speed, the ground and the view; the snowy rally's own roll is snow_roll.h's. */
#ifndef TNFS_RAIN_ROLL_H
#define TNFS_RAIN_ROLL_H

#include <stdint.h>

enum {
    RAIN_ROLL_MOVING = 0x20000,          /* 2 m/s (16.16): below it the car is stopped */
    RAIN_ROLL_CROSSED = 0xf0000,         /* 15 m/s (54 km/h): the stopped layer is gone, the moving one is all */
    RAIN_ROLL_FULL = 0x3c0000,           /* 60 m/s (216 km/h): the loudest */
    RAIN_ROLL_OUT_PERCENT = 45,          /* the stopped layer outside, of its sample's level (the samples are at -18 dBFS RMS, the roof's at -22) */
    RAIN_ROLL_IN_PERCENT = 40,           /* the stopped layer in the cockpit */
    RAIN_ROLL_MOVE_LOW_PERCENT = 140,    /* the moving layer as the cross-fade ends ... */
    RAIN_ROLL_MOVE_TOP_PERCENT = 280,    /* ... and at RAIN_ROLL_FULL and beyond (wet_roll.h takes up to WET_ROLL_MAX_PERCENT) */
    RAIN_ROLL_GRAVEL_PERCENT = 25,       /* the moving layer on the rally's gravel, percent of the asphalt's (-12 dB) */
    RAIN_ROLL_PITCH_SLOW = 800,          /* the moving sample's rate, permille, at a standstill ... */
    RAIN_ROLL_PITCH_FAST = 1300          /* ... and at RAIN_ROLL_FULL and beyond */
};

typedef struct {
    int out;     /* the stopped layer outside, percent */
    int in;      /* the stopped layer in the cockpit, percent */
    int moving;  /* the moving layer, percent */
    int pitch;   /* the moving layer's rate, permille of its own */
} RainRoll;

enum { RAIN_ROLL_DRY, RAIN_ROLL_RAIN, RAIN_ROLL_SNOW };           /* what falls */
enum { RAIN_ROLL_OFF, RAIN_ROLL_ASPHALT, RAIN_ROLL_GRAVEL };       /* the ground under the car: none (grass, a shoulder, the air), an asphalt road, the rally's gravel */

/* The three layers' levels: `falling` RAIN_ROLL_*, `open` 0..16 (weather_open), `speed` the car's (16.16 metres a second, either sign), `ground` RAIN_ROLL_OFF / _ASPHALT / _GRAVEL, `cockpit` the view
 * is the dash. All 0 when dry or the openness is 0; no stopped layer in snow. */
RainRoll rain_roll_levels(int falling, int open, int32_t speed, int ground, int cockpit);

/* The moving sample's playing rate for `speed`, permille of its own (RAIN_ROLL_PITCH_SLOW at a standstill, RAIN_ROLL_PITCH_FAST at RAIN_ROLL_FULL and beyond). */
int rain_roll_pitch(int32_t speed);

#endif
