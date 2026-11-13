/* port (rain_tyre_roll): what the followed car hears of the rain (rain_v2), three looping layers mixed over the race's own (the audio side is game/audio/wet_roll.h, one voice each):
 * - stopped, outside: the rain falling round the car (data/sounds/rain_stopped_out.wav);
 * - stopped, in the cockpit: the rain on the roof (data/sounds/rain_stopped_in.wav);
 * - moving: the wet road under the tyres (data/sounds/rain_tyre_roll.wav), on asphalt; on the rally's gravel RAIN_ROLL_GRAVEL_PERCENT of it, a layer under the exe's gravel roll (wet gravel, mud);
 *   none on grass, a shoulder or in the air.
 * Falling snow is silent (no stopped layer) but melts on the road: the moving layer is the rain's. The stopped layer gives way to the moving one with an equal-power cross-fade from RAIN_ROLL_MOVING
 * to RAIN_ROLL_CROSSED; the moving one then rises to RAIN_ROLL_FULL. Its level by the view is rain_roll_view_permille's (the cockpit 3 dB lower, the chase cameras lower the farther they
 * are), glided on a change of view by rain_roll_view_glide. The openness scales all three (0 inside a closed zone, fading at the mouths).
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
    RAIN_ROLL_PITCH_MID = 1000,          /* ... its own at RAIN_ROLL_PITCH_MID_SPEED (the sample already sounds like a middle speed) ... */
    RAIN_ROLL_PITCH_MID_SPEED = 0x190000,   /* 25 m/s (90 km/h) */
    RAIN_ROLL_PITCH_FAST = 1100,         /* ... and at RAIN_ROLL_FULL and beyond: little more */
    RAIN_ROLL_VIEW_COCKPIT_PERMILLE = 708,   /* the moving layer in the cockpit: -3 dB */
    RAIN_ROLL_VIEW_NEAR = 0x45999,       /* 4.35 m (16.16): the near chase camera's eye from the car (kind 2); 0 dB up to it (the bumper is nearer) ... */
    RAIN_ROLL_VIEW_FAR = 0x6cccc,        /* 6.8 m: the far one's (kind 3) ... */
    RAIN_ROLL_VIEW_FAR_PERMILLE = 501,   /* ... -6 dB there and beyond (the replay's cameras); in dB between */
    RAIN_ROLL_VIEW_GLIDE_TICKS = 15      /* a change of view glides with this time constant, race ticks (0.25 s) */
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
 * is the dash (which stopped layer). All 0 when dry or the openness is 0; no stopped layer in snow. The moving layer is the same for every view: the view's level is rain_roll_view_permille's. */
RainRoll rain_roll_levels(int falling, int open, int32_t speed, int ground, int cockpit);

/* The moving sample's playing rate for `speed`, permille of its own: RAIN_ROLL_PITCH_SLOW at a standstill, RAIN_ROLL_PITCH_MID at RAIN_ROLL_PITCH_MID_SPEED, RAIN_ROLL_PITCH_FAST at
 * RAIN_ROLL_FULL and beyond, straight between. */
int rain_roll_pitch(int32_t speed);

/* The moving layer's level for the view, permille: RAIN_ROLL_VIEW_COCKPIT_PERMILLE in the cockpit; outside, by `eye_distance` (the eye from the car, 16.16 metres), 1000 up to
 * RAIN_ROLL_VIEW_NEAR, RAIN_ROLL_VIEW_FAR_PERMILLE from RAIN_ROLL_VIEW_FAR on, straight in dB between. */
int rain_roll_view_permille(int cockpit, int32_t eye_distance);

/* `from` moved towards `to` (permille) over `ticks` race ticks, with the time constant RAIN_ROLL_VIEW_GLIDE_TICKS; within 15 permille (under 0.3 dB) it is `to`. */
int rain_roll_view_glide(int from, int to, uint32_t ticks);

#endif
