/* The pad as the race reads it (joy_pad.h). */
#include "game/sys/joy_pad.h"

#include "game/sys/joy_options.h"

enum { DEAD = 3, DEAD_Z_HIGH = 4, STEER_LOW = -0x20, STEER_HIGH = 0x1f, PEDAL = 0x1e, PEDAL_TOP = 0x24,
       PEDAL_DEAD = 5 };

/* the product's 32 bits, shifted down 16 (imul, sar) */
static int32_t scaled(int32_t v, int32_t scale) { return (int32_t)((uint32_t)v * (uint32_t)scale) >> 16; }

static int32_t minus(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }

/* an axis about its middle: below it to low..0, above to 0..high, each past its dead zone */
static int32_t about_mid(int32_t v, int32_t scale_low, int32_t scale_high, int32_t low, int32_t high,
                         int32_t dead_high)
{
    if (v < 0) {
        v = (int32_t)((uint32_t)scaled(v, scale_low) + DEAD);
        return v > 0 ? 0 : v < low ? low : v;
    }
    v = minus(scaled(v, scale_high), dead_high);
    return v < 0 ? 0 : v > high ? high : v;
}

/* one of the split pedals: down from PEDAL_TOP, past its dead zone */
static int32_t pedal(int32_t v, int32_t scale)
{
    const int32_t p = minus(minus(PEDAL_TOP, scaled(v >> 1, scale)), PEDAL_DEAD);
    return p < 0 ? 0 : p;
}

/* 0043d598 */
void joy_pad_read(JoyState *j, const JoyStart *s, const JoyOps *joy)
{
    if (s->present == 0 || joy_poll(j, s->device, joy) == 0) {
        j->pad_buttons = 0;
        j->pad_x = j->pad_y = 0;
        return;
    }
    j->pad_buttons = j->buttons;
    int32_t x = j->axis[0], y = j->axis[1], z = j->axis[2], r = j->axis[3];
    if (j->upright == 0) joy_turn_over(j, &x, &y, &z, &r);
    x = minus(x, j->x_mid);
    y = minus(y, j->y_mid);
    if (s->has_axis[3] == 0) z = minus(z, j->z_mid);
    x = about_mid(x, j->x_scale_low, j->x_scale_high, STEER_LOW, STEER_HIGH, DEAD);
    y = about_mid(y, j->y_scale_low, j->y_scale_high, -PEDAL, PEDAL, DEAD);
    if (s->has_axis[3] != 0) {
        const int32_t brake = pedal(r, j->r_pedal);
        const int32_t d = s->brake_wins != 0 && brake > 0 ? brake : minus(brake, pedal(z, j->z_pedal));   /* port (brake_over_gas): a brake held is the pedal, not the gas's difference */
        y = d < -PEDAL ? -PEDAL : d > PEDAL ? PEDAL : d;
    } else if (s->has_axis[2] != 0) {
        y = about_mid(z, j->z_scale_low, j->z_scale_high, -PEDAL, PEDAL, DEAD_Z_HIGH);
    }
    j->pad_x = x;
    j->pad_y = y;
}
