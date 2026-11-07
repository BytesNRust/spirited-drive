/* port (rain_tyre_roll): the wet-tyre roll a car makes on open asphalt in rain or a storm (rain_v2): a low loop (data/sounds/rain_tyre_roll.wav) whose level and pitch follow the car's speed. Pure
 * functions of the weather, the openness, the speed and the surface; the audio side is game/audio/wet_roll.h. Silent when the weather is not wet, inside a closed zone (the rain's openness 0, fading
 * at the mouths), when the car is stopped, and off the asphalt (gravel and grass have sounds of their own, as does a car in the air). */
#ifndef TNFS_RAIN_ROLL_H
#define TNFS_RAIN_ROLL_H

#include <stdint.h>

enum {
    RAIN_ROLL_MOVING = 0x20000,        /* 2 m/s (16.16): below it the car is stopped */
    RAIN_ROLL_FULL = 0x3c0000,         /* 60 m/s (216 km/h): the loudest */
    RAIN_ROLL_MAX_PERCENT = 40,        /* of the sample's level: low, under the engine */
    RAIN_ROLL_PITCH_SLOW = 800,        /* the sample's rate, permille, at the slowest ... */
    RAIN_ROLL_PITCH_FAST = 1300        /* ... and at the fastest */
};

/* The roll's level, percent 0..RAIN_ROLL_MAX_PERCENT: `wet` the weather is rain or a storm, `open` 0..16 (weather_open), `speed` the car's (16.16 metres a second, either sign), `on_asphalt` the car is
 * on the ground of an asphalt road. Rises with speed; 0 when any condition fails. */
int rain_roll_volume(int wet, int open, int32_t speed, int on_asphalt);

/* The sample's playing rate for `speed`, permille of its own (RAIN_ROLL_PITCH_SLOW at a standstill, RAIN_ROLL_PITCH_FAST at RAIN_ROLL_FULL and beyond). */
int rain_roll_pitch(int32_t speed);

#endif
