/* The in-car dashboard's full redraw (00421160). */
#include "game/race/cockpit.h"

#include "game/gfx/blit.h"
#include "game/gfx/flat.h"

enum {
    GRAB_X_HIGH = 0x208, GRAB_Y_HIGH = 0x132, GRAB_X_LOW = 0x104, GRAB_Y_LOW = 0x7f,
    BOX_W_HIGH = 0x70, BOX_H_HIGH = 0x8b, BOX_W_LOW = 0x38, BOX_H_LOW = 0x3b,
    SHADES = 2,
    MODE_REDRAWN = 99
};

static QuadTarget *select_screen(const CockpitRedrawOps *ops, const CockpitRedrawInputs *in)
{
    return ops->select(ops->ctx, in->back_buffer != 0 ? HUD_SURFACE_SCREEN : HUD_SURFACE_DEFAULT);
}

void cockpit_redraw(const CockpitRedrawOps *ops, HudState *hud, const Cockpit *c, CockpitWorld *w,
                    const CockpitRedrawInputs *in)
{
    const int high = in->view_clip.high_res != 0;
    QuadTarget *t = select_screen(ops, in);
    hud_view_clip(t, hud, &in->view_clip);
    blit_shape_at_transparent(t, c->dash, 0, 0);
    w->back_saved = 1;
    blit_grab(t, in->back_shape, high ? GRAB_X_HIGH : GRAB_X_LOW, high ? GRAB_Y_HIGH : GRAB_Y_LOW);
    const QuadTarget *back = ops->select(ops->ctx, HUD_SURFACE_BACK);
    const int32_t bw = high ? BOX_W_HIGH : BOX_W_LOW, bh = high ? BOX_H_HIGH : BOX_H_LOW;
    const int32_t box[8] = { 0, 0, bw, 0, bw, bh, 0, bh };
    for (int k = 0; k < SHADES; k++) flat_polygon_shade(back, in->remap, 4, box, 0);
    select_screen(ops, in);
    ops->labels(ops->ctx);
    hud->unknown_4f9a20[0] = MODE_REDRAWN;
    hud->unknown_4f9a20[1] = 0;
    hud_reset(hud);
}
