/* port (snow crunch): see snow_roll.h. */
#include "game/race/snow_roll.h"

#include "game/race/rally_gravel.h"

static int32_t magnitude(int32_t speed) { return speed < 0 ? -speed : speed; }

/* the gravel roll's floor at `speed` (rally_gravel_tyre_skid's, 0 below RALLY_TYRE_MOVING metres a second) */
static int roll_floor(int32_t speed)
{
    const int32_t metres = magnitude(speed) >> 16;
    if (metres < RALLY_TYRE_MOVING) return 0;
    const int32_t roll = metres * RALLY_TYRE_PER_METRE_X2 / 2;
    return roll > RALLY_TYRE_TOP ? RALLY_TYRE_TOP : roll;
}

int snow_roll_volume(int kind, int32_t speed, int on_ground)
{
    if (kind != RALLY_GRAVEL_SNOW || !on_ground) return 0;
    return SNOW_ROLL_MAX_PERCENT * roll_floor(speed) / RALLY_TYRE_TOP;
}

int snow_roll_pitch(int32_t speed)
{
    return SNOW_ROLL_PITCH_SLOW + (SNOW_ROLL_PITCH_FAST - SNOW_ROLL_PITCH_SLOW) * roll_floor(speed) / RALLY_TYRE_TOP;
}
