/* The calibration's sums (joy_calibrate.h). */
#include "game/sys/joy_calibrate.h"

enum {
    TRAVEL_MAX = 0x7ffa,
    STEER_RANGE = 0x23,  /* the pad's -0x20..0x1f with its 3 dead */
    Z_HIGH_RANGE = 0x26,
    PEDAL_RANGE = 0x24
};

static int32_t at_most(int32_t v) { return v > TRAVEL_MAX ? TRAVEL_MAX : v; }

/* range over travel + 1 in 16.16, a travel of 1 or less left as it is; the
 * divisor is the exe's 32-bit one (a travel of 0xffff divides by 0 there, and
 * is left here) */
static int32_t scale(int32_t travel, int32_t range)
{
    const int32_t by = (int32_t)((uint32_t)travel * 0x10000u + 0x10000u);
    if (travel <= 1 || by == 0) return travel;
    return (int32_t)(((int64_t)range << 32) / by);
}

void joy_calibrate(JoyState *j, int32_t pedals)
{
    const int32_t y_low = j->y_mid - j->y_min, y_high = j->y_max - j->y_mid;
    const int32_t x_low = j->x_mid - j->x_min, x_high = j->x_max - j->x_mid;
    j->y_low = (y_low >> 1) + j->y_min;
    j->y_high = (y_high >> 1) + j->y_mid;
    j->x_low = (x_low >> 1) + j->x_min;
    j->x_high = (x_high >> 1) + j->x_mid;
    j->z_pedal = ((j->z_max - j->z_min) >> 1) - 1;
    j->r_pedal = ((j->r_max - j->r_min) >> 1) - 1;
    j->x_scale_low = scale(at_most(x_low), STEER_RANGE);
    j->x_scale_high = scale(at_most(x_high), STEER_RANGE);
    j->y_scale_low = scale(at_most(y_low), STEER_RANGE);
    j->y_scale_high = scale(at_most(y_high), STEER_RANGE);
    j->z_scale_low = scale(at_most(j->z_mid - j->z_min), STEER_RANGE);
    j->z_scale_high = scale(j->z_max - j->z_mid, Z_HIGH_RANGE);
    if (pedals != 0) {
        j->z_pedal = scale(j->z_pedal, PEDAL_RANGE);
        j->r_pedal = scale(j->r_pedal, PEDAL_RANGE);
    }
}
