#include "game/race/rain_roll.h"

#include <math.h>

enum { OPEN_FULL = 16, RAIN_ROLL_VIEW_GLIDE_SNAP = 15 };   /* permille */

static int32_t magnitude(int32_t speed) { return speed < 0 ? -speed : speed; }

/* where `v` is between `from` and `to`, 0..1 */
static double between(int32_t v, int32_t from, int32_t to) { return v <= from ? 0.0 : v >= to ? 1.0 : (double)(v - from) / (double)(to - from); }

static int scaled(double percent, int open) { return (int)lround(percent * (open > OPEN_FULL ? OPEN_FULL : open) / OPEN_FULL); }

/* the equal-power cross-fade by speed: the moving layers' share (the stopped ones' is cos of the same angle) */
static double fade(int32_t v) { return between(v, RAIN_ROLL_MOVING, RAIN_ROLL_CROSSED) * (M_PI / 2); }

/* a moving layer's level: `low` as the cross-fade ends, `top` at RAIN_ROLL_FULL, by the cross-fade below */
static double rising(int32_t v, int low, int top) { return (low + (top - low) * between(v, RAIN_ROLL_CROSSED, RAIN_ROLL_FULL)) * sin(fade(v)); }

int rain_roll_spray(int open, int32_t speed, int ground)
{
    if (open <= 0 || (ground != RAIN_ROLL_ASPHALT && ground != RAIN_ROLL_GRAVEL)) return 0;
    const double under = ground == RAIN_ROLL_GRAVEL ? RAIN_ROLL_GRAVEL_PERCENT / 100.0 : 1.0;   /* under the gravel roll */
    return scaled(rising(magnitude(speed), RAIN_ROLL_SPRAY_LOW_PERCENT, RAIN_ROLL_SPRAY_TOP_PERCENT) * under, open);
}

RainRoll rain_roll_levels(int falling, int open, int32_t speed, int ground, int cockpit)
{
    RainRoll r = { 0, 0, 0, 0, 0, rain_roll_pitch(speed) };
    if ((falling != RAIN_ROLL_RAIN && falling != RAIN_ROLL_SNOW) || open <= 0) return r;
    const int32_t v = magnitude(speed);
    if (falling == RAIN_ROLL_RAIN) {   /* falling snow is silent */
        const double stopped = cos(fade(v));
        if (cockpit) {
            r.in = scaled(RAIN_ROLL_IN_PERCENT * stopped, open);
            r.move_in = scaled(rising(v, RAIN_ROLL_MOVE_IN_LOW_PERCENT, RAIN_ROLL_MOVE_IN_TOP_PERCENT), open);
            return r;   /* the car's own spray is in the cockpit's moving layer */
        }
        r.out = scaled(RAIN_ROLL_OUT_PERCENT * stopped, open);
        r.bed = scaled(RAIN_ROLL_BED_PERCENT * sin(fade(v)), open);
    }
    r.spray = rain_roll_spray(open, speed, ground);
    return r;
}

int rain_roll_pitch(int32_t speed)
{
    const int32_t v = magnitude(speed);
    if (v <= RAIN_ROLL_PITCH_MID_SPEED) return (int)lround(RAIN_ROLL_PITCH_SLOW + (RAIN_ROLL_PITCH_MID - RAIN_ROLL_PITCH_SLOW) * between(v, 0, RAIN_ROLL_PITCH_MID_SPEED));
    return (int)lround(RAIN_ROLL_PITCH_MID + (RAIN_ROLL_PITCH_FAST - RAIN_ROLL_PITCH_MID) * between(v, RAIN_ROLL_PITCH_MID_SPEED, RAIN_ROLL_FULL));
}

int rain_roll_view_permille(int cockpit, int32_t eye_distance)
{
    const double far_db = 20.0 * log10(RAIN_ROLL_VIEW_FAR_PERMILLE / 1000.0);
    double level = 1000.0 * pow(10.0, far_db * between(eye_distance, RAIN_ROLL_VIEW_NEAR, RAIN_ROLL_VIEW_FAR) / 20.0);
    if (eye_distance > RAIN_ROLL_VIEW_FAR) level *= (double)RAIN_ROLL_VIEW_FAR / eye_distance * (1.0 - between(eye_distance, RAIN_ROLL_HEARD_FADE, RAIN_ROLL_HEARD));
    return (int)lround(cockpit ? level * RAIN_ROLL_VIEW_COCKPIT_PERMILLE / 1000.0 : level);
}

int rain_roll_view_glide(int from, int to, uint32_t ticks)
{
    const double left = (from - to) * exp(-(double)ticks / RAIN_ROLL_VIEW_GLIDE_TICKS);
    return fabs(left) < RAIN_ROLL_VIEW_GLIDE_SNAP ? to : to + (int)(left > 0 ? floor(left) : ceil(left));   /* the last step (under 0.3 dB) taken at once: it arrives */
}
