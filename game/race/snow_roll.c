/* port (snow): see snow_roll.h. */
#include "game/race/snow_roll.h"

enum { OPEN_FULL = 16 };

static int32_t magnitude(int32_t speed) { return speed < 0 ? -speed : speed; }

/* how far `speed` is from moving to full, 0..65536 */
static int32_t rise(int32_t speed)
{
    const int32_t v = magnitude(speed);
    if (v <= SNOW_ROLL_MOVING) return 0;
    if (v >= SNOW_ROLL_FULL) return 0x10000;
    return (int32_t)((int64_t)(v - SNOW_ROLL_MOVING) * 0x10000 / (SNOW_ROLL_FULL - SNOW_ROLL_MOVING));
}

int snow_roll_volume(int snow, int open, int32_t speed, int on_ground, int cockpit)
{
    if (!snow || !on_ground || open <= 0) return 0;
    const int level = (int)((int64_t)SNOW_ROLL_MAX_PERCENT * rise(speed) / 0x10000);
    return level * (open > OPEN_FULL ? OPEN_FULL : open) / OPEN_FULL / (cockpit ? 2 : 1);   /* the cockpit: 6 dB lower */
}

int snow_roll_pitch(int32_t speed)
{
    return SNOW_ROLL_PITCH_SLOW + (int)((int64_t)(SNOW_ROLL_PITCH_FAST - SNOW_ROLL_PITCH_SLOW) * rise(speed) / 0x10000);
}
