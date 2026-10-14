/* The HUD's frame outside the car (hud_frame.h), as 00422598 runs it. */
#include "game/race/hud_frame.h"

enum { NOTICE_TICK_IDLE = -1000 };

/* 0042266d..0042267b and the three like it: the page put back without a back buffer. */
static void settle(const HudFrameOps *ops, const HudFrameInputs *in)
{
    if (in->back_buffer != 0) return;
    ops->restore(ops->ctx);
    ops->select(ops->ctx, HUD_SURFACE_DEFAULT);
}

/* 00422605..00422654 */
static void full_redraw(const HudFrameOps *ops, const HudFrameInputs *in)
{
    if (in->back_buffer != 0) {
        ops->clear_pages(ops->ctx);
        ops->select(ops->ctx, HUD_SURFACE_SCREEN);
        ops->tiles(ops->ctx);
        ops->select_front(ops->ctx);
        ops->show(ops->ctx);
        ops->video_flip(ops->ctx);
    } else {
        ops->select(ops->ctx, HUD_SURFACE_DEFAULT);
        ops->tiles(ops->ctx);
    }
}

void hud_frame(const HudFrameOps *ops, const HudFrameWords *w, const HudFrameInputs *in, int32_t changed)
{
    if (in->unknown_509d70 != 0) return;
    if (in->ticket != 0 || in->warning != 0) {
        ops->police(ops->ctx);
        ops->reselect(ops->ctx);
        ops->panel(ops->ctx);
        *w->redraw = 1;
        return;
    }
    *w->loading = 1;
    ops->reload(ops->ctx);
    *w->loading = in->unknown_509d70;
    *w->notice_tick = NOTICE_TICK_IDLE;
    if (*w->redraw != 0) {
        full_redraw(ops, in);
        *w->redraw = 0;
    }
    ops->mask(ops->ctx, changed);
    ops->keys(ops->ctx);
    ops->top_row(ops->ctx);
    settle(ops, in);
    ops->needle(ops->ctx);
    settle(ops, in);
    if (in->use_parts != 0) {
        if (in->high != 0)
            ops->map(ops->ctx, HUD_FRAME_MAP_X_HIGH, HUD_FRAME_MAP_Y_HIGH);
        else
            ops->map(ops->ctx, HUD_FRAME_MAP_X_LOW, HUD_FRAME_MAP_Y_LOW);
    }
    settle(ops, in);
    ops->panel(ops->ctx);
    settle(ops, in);
}
