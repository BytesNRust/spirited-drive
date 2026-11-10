#include "game/race/spray_pace.h"

#include "game/race/tick_rate.h"

#include <math.h>
#include <string.h>

void spray_pace_clear(SprayPace *pace)
{
    memset(pace, 0, sizeof *pace);
}

int32_t spray_pace(SprayPace *pace, int32_t tick, int32_t x, int32_t z, int32_t stored)
{
    if (pace->has_anchor && tick < pace->last_tick) spray_pace_clear(pace);   /* the clock went back */
    if (pace->has_anchor && tick == pace->last_tick) return pace->last_pace;
    const int64_t own = stored < 0 ? -(int64_t)stored : stored;
    if (!pace->has_anchor) {
        pace->x = x, pace->z = z, pace->tick = tick, pace->measured = 0, pace->has_anchor = 1;
    } else if (tick - pace->tick >= SPRAY_PACE_WINDOW) {
        const double dx = ((double)x - (double)pace->x) / 65536.0, dz = ((double)z - (double)pace->z) / 65536.0;
        const double metres = sqrt(dx * dx + dz * dz), seconds = (double)(tick - pace->tick) / (double)RACE_TICKS_A_SECOND;
        pace->measured = metres > (double)SPRAY_PACE_TELEPORT_M ? 0 : (int32_t)(metres / seconds * 65536.0);
        pace->x = x, pace->z = z, pace->tick = tick;
    }
    const int64_t allowed = 2 * (int64_t)pace->measured + (int64_t)SPRAY_PACE_SLACK_MS * 65536;
    pace->last_tick = tick;
    pace->last_pace = (int32_t)(own < allowed ? own : allowed);
    return pace->last_pace;
}
