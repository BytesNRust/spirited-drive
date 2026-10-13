/* Steering by the mouse (mouse_steer.h). */
#include "game/race/mouse_steer.h"

#include "game/gfx/x86_int.h"

enum { CENTRE = 0x20, RANGE_X = 0x100, RANGE_Y = 0x40, DEAD_LOW = 0x1d, DEAD_HIGH = 0x22, FULL = 0x3c };

void mouse_steer(const MouseSteerOps *ops, MouseSteerWords *w)
{
    if (w->replay != 0 || w->viewed != w->player || w->unknown_513e6c != 0) {
        w->steer = CENTRE;
        return;
    }
    ops->range(ops->ctx, 0, 0, RANGE_X, RANGE_Y);
    w->buttons = ops->buttons(ops->ctx);
    const int32_t x = x_sar(ops->x(ops->ctx), 2);
    w->steer = w->mirrored != 0 ? x_sub(RANGE_Y, x) : x;
    if (w->steer > DEAD_LOW && w->steer < DEAD_HIGH) w->steer = CENTRE;
    else if (w->steer > FULL) w->steer = FULL;
    else if (w->steer < 0) w->steer = 0;
}
