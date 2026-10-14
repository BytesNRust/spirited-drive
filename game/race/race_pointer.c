#include "game/race/race_pointer.h"

static void select_screen(const RacePointerOps *ops, int32_t back_buffer)
{
    ops->select(ops->ctx, back_buffer != 0 ? HUD_SURFACE_SCREEN : HUD_SURFACE_DEFAULT);
}

void race_pointer_show(const RacePointerOps *ops, const RacePointerInputs *in, RacePointerKept *kept, int32_t pointer)
{
    select_screen(ops, in->back_buffer);
    ops->grab(ops->ctx, in->shapes[0], in->x, in->y);
    ops->draw(ops->ctx, in->shapes[1 + pointer], in->x, in->y);
    kept->x = (int16_t)in->x;
    kept->y = (int16_t)in->y;
    kept->phase = in->phase;
}
