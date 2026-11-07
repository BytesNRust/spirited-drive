#include "game/race/rain_roll.h"

enum { OPEN_FULL = 16 };

static int32_t magnitude(int32_t speed) { return speed < 0 ? -speed : speed; }

int rain_roll_volume(int wet, int open, int32_t speed, int on_asphalt)
{
    const int32_t v = magnitude(speed);
    if (!wet || !on_asphalt || open <= 0 || v <= RAIN_ROLL_MOVING) return 0;
    const int32_t span = RAIN_ROLL_FULL - RAIN_ROLL_MOVING, up = v >= RAIN_ROLL_FULL ? span : v - RAIN_ROLL_MOVING;
    const int level = (int)((int64_t)RAIN_ROLL_MAX_PERCENT * up / span);   /* rises with speed */
    return level * (open > OPEN_FULL ? OPEN_FULL : open) / OPEN_FULL;
}

int rain_roll_pitch(int32_t speed)
{
    const int32_t v = magnitude(speed) > RAIN_ROLL_FULL ? RAIN_ROLL_FULL : magnitude(speed);
    return RAIN_ROLL_PITCH_SLOW + (int)((int64_t)(RAIN_ROLL_PITCH_FAST - RAIN_ROLL_PITCH_SLOW) * v / RAIN_ROLL_FULL);
}
