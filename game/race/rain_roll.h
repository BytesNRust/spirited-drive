/* port (rain_tyre_roll): what is heard of the rain (rain_v2), looping layers mixed over the race's own (the audio side is game/audio/wet_roll.h, one voice each):
 * - stopped, outside: the rain falling round the car (data/sounds/rain_stopped_out.wav);
 * - stopped, in the cockpit: the rain on the roof (data/sounds/rain_stopped_in.wav);
 * - moving, outside: the rain round a moving car (data/sounds/rain_moving_out.wav), once, whatever cars there are;
 * - moving, in the cockpit: the rain on the roof and the windscreen with the hiss of the car's own tyres (data/sounds/rain_moving_in.wav);
 * - every car's tyre spray (data/sounds/rain_spray.wav), a source of its own where the car is (rain_cars.h), on asphalt; on the rally's gravel RAIN_ROLL_GRAVEL_PERCENT of it, a layer
 *   under the exe's gravel roll (wet gravel, mud); none on grass, a shoulder or in the air. The followed car's own spray is not heard in the cockpit in rain: rain_moving_in.wav has it.
 * Falling snow is silent (no rain layer) but melts on the road: the spray is the rain's, in the cockpit too. Each stopped layer gives way to its moving one with an equal-power cross-fade
 * from RAIN_ROLL_MOVING to RAIN_ROLL_CROSSED; the cockpit's moving layer and the spray then rise to their top at RAIN_ROLL_FULL. A spray's level by where it is heard from is
 * rain_roll_view_permille's (the cockpit 3 dB lower, lower the farther the eye), the followed car's glided on a change of view by rain_roll_view_glide. The openness scales all
 * (0 inside a closed zone, fading at the mouths). Pure functions of what falls, the openness, the speed, the ground and the view; the snowy rally's own roll is snow_roll.h's. */
#ifndef TNFS_RAIN_ROLL_H
#define TNFS_RAIN_ROLL_H

#include <stdint.h>

enum {
    RAIN_ROLL_MOVING = 0x20000,          /* 2 m/s (16.16): below it the car is stopped */
    RAIN_ROLL_CROSSED = 0xf0000,         /* 15 m/s (54 km/h): the stopped layers are gone, the moving ones are all */
    RAIN_ROLL_FULL = 0x3c0000,           /* 60 m/s (216 km/h): the loudest */
    RAIN_ROLL_OUT_PERCENT = 45,          /* the stopped layer outside, of its sample's level (-18 dBFS RMS: -25 dBFS) */
    RAIN_ROLL_IN_PERCENT = 40,           /* the stopped layer in the cockpit (-22 dBFS RMS: -30 dBFS) */
    RAIN_ROLL_BED_PERCENT = 68,          /* the moving layer outside (-21.6 dBFS RMS): the stopped one's level, -25 dBFS */
    RAIN_ROLL_MOVE_IN_LOW_PERCENT = 111, /* the moving layer in the cockpit (-19 dBFS RMS) as the cross-fade ends ... */
    RAIN_ROLL_MOVE_IN_TOP_PERCENT = 221, /* ... and at RAIN_ROLL_FULL and beyond: -18 to -12 dBFS, the level the wet road had there */
    RAIN_ROLL_SPRAY_LOW_PERCENT = 260,   /* a car's spray (-23.4 dBFS RMS) as the cross-fade ends ... */
    RAIN_ROLL_SPRAY_TOP_PERCENT = 519,   /* ... and at RAIN_ROLL_FULL and beyond: -15 to -9 dBFS near the car (wet_roll.h takes up to WET_ROLL_MAX_PERCENT) */
    RAIN_ROLL_GRAVEL_PERCENT = 25,       /* the spray on the rally's gravel, percent of the asphalt's (-12 dB) */
    RAIN_ROLL_PITCH_SLOW = 800,          /* the spray's rate, permille, at a standstill ... */
    RAIN_ROLL_PITCH_MID = 1000,          /* ... its own at RAIN_ROLL_PITCH_MID_SPEED (the sample already sounds like a middle speed) ... */
    RAIN_ROLL_PITCH_MID_SPEED = 0x190000,   /* 25 m/s (90 km/h) */
    RAIN_ROLL_PITCH_FAST = 1100,         /* ... and at RAIN_ROLL_FULL and beyond: little more */
    RAIN_ROLL_VIEW_COCKPIT_PERMILLE = 708,   /* a spray heard from the cockpit: -3 dB */
    RAIN_ROLL_VIEW_NEAR = 0x45999,       /* 4.35 m (16.16): the near chase camera's eye from the car (kind 2); 0 dB up to it (the bumper is nearer) ... */
    RAIN_ROLL_VIEW_FAR = 0x6cccc,        /* 6.8 m: the far one's (kind 3) ... */
    RAIN_ROLL_VIEW_FAR_PERMILLE = 501,   /* ... -6 dB there, in dB between; beyond, -6 dB each time the distance doubles ... */
    RAIN_ROLL_HEARD_FADE = 0x280000,     /* ... and from 40 m ... */
    RAIN_ROLL_HEARD = 0x3c0000,          /* ... down to nothing at 60 m */
    RAIN_ROLL_VIEW_GLIDE_TICKS = 15      /* a change of view glides with this time constant, race ticks (0.25 s) */
};

typedef struct {
    int out;      /* stopped, outside, percent */
    int in;       /* stopped, in the cockpit */
    int bed;      /* moving, outside (once) */
    int move_in;  /* moving, in the cockpit */
    int spray;    /* the followed car's own spray, before rain_roll_view_permille; 0 in the cockpit in rain (move_in has it) */
    int pitch;    /* the spray's rate, permille of its own */
} RainRoll;

enum { RAIN_ROLL_DRY, RAIN_ROLL_RAIN, RAIN_ROLL_SNOW };           /* what falls */
enum { RAIN_ROLL_OFF, RAIN_ROLL_ASPHALT, RAIN_ROLL_GRAVEL };       /* the ground under the car: none (grass, a shoulder, the air), an asphalt road, the rally's gravel */

/* The layers' levels for the followed car: `falling` RAIN_ROLL_*, `open` 0..16 (weather_open), `speed` the car's (16.16 metres a second, either sign), `ground` RAIN_ROLL_OFF / _ASPHALT /
 * _GRAVEL, `cockpit` the view is the dash. All 0 when dry or the openness is 0; in snow the spray alone. The rain's layers do not depend on the ground: only the spray does. */
RainRoll rain_roll_levels(int falling, int open, int32_t speed, int ground, int cockpit);

/* Any car's spray, percent, where rain or snow falls: 0 below RAIN_ROLL_MOVING, rising with the equal-power fade to RAIN_ROLL_SPRAY_LOW_PERCENT at RAIN_ROLL_CROSSED and straight to
 * RAIN_ROLL_SPRAY_TOP_PERCENT at RAIN_ROLL_FULL; by the ground (none off it, a quarter on gravel) and the openness at the car. */
int rain_roll_spray(int open, int32_t speed, int ground);

/* The spray's playing rate for `speed`, permille of its own: RAIN_ROLL_PITCH_SLOW at a standstill, RAIN_ROLL_PITCH_MID at RAIN_ROLL_PITCH_MID_SPEED, RAIN_ROLL_PITCH_FAST at
 * RAIN_ROLL_FULL and beyond, straight between. */
int rain_roll_pitch(int32_t speed);

/* A spray's level heard from `eye_distance` (the eye from the car, 16.16 metres), permille: 1000 up to RAIN_ROLL_VIEW_NEAR, RAIN_ROLL_VIEW_FAR_PERMILLE at RAIN_ROLL_VIEW_FAR (straight
 * in dB between), falling as 1/distance beyond, faded to 0 from RAIN_ROLL_HEARD_FADE to RAIN_ROLL_HEARD; RAIN_ROLL_VIEW_COCKPIT_PERMILLE of that from the cockpit. */
int rain_roll_view_permille(int cockpit, int32_t eye_distance);

/* `from` moved towards `to` (permille) over `ticks` race ticks, with the time constant RAIN_ROLL_VIEW_GLIDE_TICKS; within 15 permille (under 0.3 dB) it is `to`. */
int rain_roll_view_glide(int from, int to, uint32_t ticks);

#endif
