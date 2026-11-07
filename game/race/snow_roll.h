/* port (snow crunch): the roll of a car on the snowy rally (RALLY_GRAVEL_SNOW, Alpine 3): the crunch of packed snow (data/sounds/snow_crunch.wav) in place of the exe's gravel sample, which the snow's rally no
 * longer strikes. Pure functions of the rally's kind, the speed and whether the car is on the ground; the audio side is game/audio/wet_roll.h (the same looping voice as the wet-tyre roll's). Its level follows the
 * gravel roll's own curve (rally_gravel_tyre_skid: 1.5 a metre a second, from RALLY_TYRE_MOVING, up to RALLY_TYRE_TOP), and its pitch rises with the speed the same way. */
#ifndef TNFS_SNOW_ROLL_H
#define TNFS_SNOW_ROLL_H

#include <stdint.h>

enum {
    SNOW_ROLL_MAX_PERCENT = 70,   /* of the sample's level, at the top of the gravel roll's curve */
    SNOW_ROLL_PITCH_SLOW = 850,   /* the sample's rate, permille, at a walking pace ... */
    SNOW_ROLL_PITCH_FAST = 1250   /* ... and at the top of the curve and beyond */
};

/* The level, percent 0..SNOW_ROLL_MAX_PERCENT: `kind` the rally's gravel kind (RALLY_GRAVEL_*; only the snow's makes it), `speed` the car's (16.16 metres a second, either sign), `on_ground` the car is not in the
 * air. 0 for another kind, a standing car or one in the air. */
int snow_roll_volume(int kind, int32_t speed, int on_ground);

/* The sample's playing rate for `speed`, permille of its own. */
int snow_roll_pitch(int32_t speed);

#endif
