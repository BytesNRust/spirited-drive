/* port (snow): the roll of the followed car on the snowy rally's track (RALLY_GRAVEL_SNOW, Alpine 3; snow falling elsewhere is the rain's wet road, rain_roll.h): a looping drag over snow (data/sounds/snow_roll.wav) that sounds like a
 * friction rising with speed, its level and pitch rising more steeply than the wet roll's (rain_roll.h). Silent when the car stands or is in the air, and where nothing falls (the weather's openness 0,
 * a closed zone); the cockpit hears it 6 dB lower. The snowy rally's gravel sample is not struck (race_sound.c). Pure functions; the audio side is game/audio/wet_roll.h (the same looping voice). */
#ifndef TNFS_SNOW_ROLL_H
#define TNFS_SNOW_ROLL_H

#include <stdint.h>

enum {
    SNOW_ROLL_MOVING = 0x20000,    /* 2 m/s (16.16): below it the car is standing, silent */
    SNOW_ROLL_FULL = 0x320000,     /* 50 m/s (180 km/h): the loudest and highest */
    SNOW_ROLL_MAX_PERCENT = 90,    /* of the sample's level (-18 dBFS RMS), outside, at SNOW_ROLL_FULL and beyond */
    SNOW_ROLL_PITCH_SLOW = 700,    /* the sample's rate, permille, as it starts to move ... */
    SNOW_ROLL_PITCH_FAST = 1600    /* ... and at SNOW_ROLL_FULL and beyond */
};

/* The level, percent 0..SNOW_ROLL_MAX_PERCENT: `snow` the car is on the snowy rally's track, `open` 0..16 (weather_open where snow falls, else 16), `speed` the car's (16.16 metres a second, either
 * sign), `on_ground` the car is not in the air, `cockpit` the view is the dash. Rises from 0 at SNOW_ROLL_MOVING to the top at SNOW_ROLL_FULL. */
int snow_roll_volume(int snow, int open, int32_t speed, int on_ground, int cockpit);

/* The sample's playing rate for `speed`, permille of its own (SNOW_ROLL_PITCH_SLOW up to SNOW_ROLL_MOVING, SNOW_ROLL_PITCH_FAST at SNOW_ROLL_FULL and beyond). */
int snow_roll_pitch(int32_t speed);

#endif
