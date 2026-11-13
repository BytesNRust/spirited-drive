#include "game/race/rain_roll.h"

#include <math.h>

enum { OPEN_FULL = 16 };

static int32_t magnitude(int32_t speed) { return speed < 0 ? -speed : speed; }

/* where `v` is between `from` and `to`, 0..1 */
static double between(int32_t v, int32_t from, int32_t to) { return v <= from ? 0.0 : v >= to ? 1.0 : (double)(v - from) / (double)(to - from); }

static int scaled(double percent, int open) { return (int)lround(percent * (open > OPEN_FULL ? OPEN_FULL : open) / OPEN_FULL); }

RainRoll rain_roll_levels(int falling, int open, int32_t speed, int ground, int cockpit)
{
    RainRoll r = { 0, 0, 0, rain_roll_pitch(speed) };
    if ((falling != RAIN_ROLL_RAIN && falling != RAIN_ROLL_SNOW) || open <= 0) return r;
    const int32_t v = magnitude(speed);
    const double fade = between(v, RAIN_ROLL_MOVING, RAIN_ROLL_CROSSED) * (M_PI / 2);   /* equal power: the two gains' squares sum to 1 */
    const double stopped = cos(fade), moving = sin(fade);
    if (falling == RAIN_ROLL_RAIN) {   /* falling snow is silent */
        if (cockpit) r.in = scaled(RAIN_ROLL_IN_PERCENT * stopped, open);
        else r.out = scaled(RAIN_ROLL_OUT_PERCENT * stopped, open);
    }
    if (ground == RAIN_ROLL_ASPHALT || ground == RAIN_ROLL_GRAVEL) {
        const double rise = RAIN_ROLL_MOVE_LOW_PERCENT + (RAIN_ROLL_MOVE_TOP_PERCENT - RAIN_ROLL_MOVE_LOW_PERCENT) * between(v, RAIN_ROLL_CROSSED, RAIN_ROLL_FULL);
        const double under = ground == RAIN_ROLL_GRAVEL ? RAIN_ROLL_GRAVEL_PERCENT / 100.0 : 1.0;   /* under the gravel roll */
        r.moving = scaled(rise * moving * under * (cockpit ? 0.5 : 1.0), open);                       /* the cockpit: 6 dB lower */
    }
    return r;
}

int rain_roll_pitch(int32_t speed)
{
    const int32_t v = magnitude(speed) > RAIN_ROLL_FULL ? RAIN_ROLL_FULL : magnitude(speed);
    return RAIN_ROLL_PITCH_SLOW + (int)((int64_t)(RAIN_ROLL_PITCH_FAST - RAIN_ROLL_PITCH_SLOW) * v / RAIN_ROLL_FULL);
}
