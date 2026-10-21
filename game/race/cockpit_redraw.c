/* The in-car dashboard's full redraw (00421160) and its reload on a view
 * change (004217a8). */
#include "game/race/cockpit.h"

#include "game/gfx/blit.h"
#include "game/gfx/flat.h"
#include "game/sys/shape_bank.h"

#include <stddef.h>
#include <stdio.h>

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
    if (ops->port_grab != NULL) ops->port_grab(ops->ctx, in->back_shape);
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

static QuadTarget *select_back_buffer(const CockpitReloadOps *ops, int32_t back_buffer)
{
    return ops->select(ops->ctx, back_buffer != 0 ? HUD_SURFACE_SCREEN : HUD_SURFACE_DEFAULT);
}

void cockpit_reload(const CockpitReloadOps *ops, Cockpit *c, CockpitReloadState *st, const CockpitReloadInputs *in)
{
    if (c->bank != NULL) return;
    if (st->unknown_4f9620 != 0) {
        ops->free_saved(ops->ctx);
        st->unknown_4f9620 = 0;
    }
    if (in->back_buffer != 0) ops->flip(ops->ctx);
    const QuadTarget *t = select_back_buffer(ops, in->back_buffer);
    flat_fill_rect(t, t->clip_left, t->clip_top, t->clip_right - t->clip_left, t->clip_bottom - t->clip_top, 0);
    if (in->back_buffer == 0) ops->restore(ops->ctx);
    ops->load(ops->ctx);
    if (in->unknown_509d70 != 0) return;
    ops->palette_copy(ops->ctx);
    ops->palette_bank(ops->ctx, c->bank, c->palette);
    select_back_buffer(ops, in->back_buffer);
    if (in->unknown_4b92dc == 0 && in->unknown_4b92e0 == 0 && in->dash_camera == 0 && in->chase_kind == 0 &&
        in->unknown_4b94dc == 0)
        ops->redraw(ops->ctx);
    if (in->unknown_4c5390 == 1) st->unknown_52b8f8 = 1;
    else st->unknown_513898 = 1;
}

enum { POLICE_PATH = 80, PALETTE_AT = 0x10 };

/* 004adea4..004adf3c by ticket, german, driving ('D', 004c5390 == 1) and high resolution */
static const char *police_name(const CockpitPoliceInputs *in)
{
    static const char *const names[2][2][2][2] = {
        { { { "warnRL", "warnRH" }, { "warnDL", "warnDH" } }, { { "gwarnRL", "gwarnRH" }, { "gwarnDL", "gwarnDH" } } },
        { { { "ticketRL", "ticketRH" }, { "ticketDL", "ticketDH" } },
          { { "tickegRL", "tickegRH" }, { "tickegDL", "tickegDH" } } }
    };
    return names[in->ticket != 0][in->german != 0][in->view_clip.unknown_4c5390 == 1][in->view_clip.high_res != 0];
}

static int police_load(const CockpitPoliceOps *ops, Cockpit *c, CockpitPoliceState *st, const CockpitPoliceInputs *in)
{
    st->loading = 1;
    if (c->bank != NULL) {
        ops->free_bank(ops->ctx, c->bank);
        c->bank = NULL;
    }
    char path[POLICE_PATH];
    const int n = snprintf(path, sizeof path, "%s%s.qfs", in->dir, police_name(in));
    if (n < 0 || n >= (int)sizeof path) return -1;
    st->screen_bank = ops->load(ops->ctx, path);
    if (st->screen_bank == NULL) return -1;
    st->screen = shape_bank_find(st->screen_bank, (const uint8_t *)"scrn");
    if (st->screen == NULL) return -1;
    ops->palette_read(ops->ctx);
    const uint8_t *pal = shape_bank_find(st->screen_bank, (const uint8_t *)"!pal");
    if (pal == NULL) return -1;
    if (in->view_clip.unknown_4c5390 == 1) ops->palette_write(ops->ctx, pal + PALETTE_AT);
    else ops->palette_bank(ops->ctx, st->screen_bank, pal + PALETTE_AT);
    st->loading = 0;
    return 0;
}

int cockpit_police(const CockpitPoliceOps *ops, Cockpit *c, CockpitPoliceState *st, HudState *hud,
                   const CockpitPoliceInputs *in)
{
    if (st->screen_bank == NULL && police_load(ops, c, st, in) != 0) return -1;
    if (st->screen == NULL) return -1;
    QuadTarget *t = ops->select(ops->ctx, in->back_buffer != 0 ? HUD_SURFACE_SCREEN : HUD_SURFACE_DEFAULT);
    if (in->unknown_524e63 != 0) ops->movie(ops->ctx);
    hud_view_clip(t, hud, &in->view_clip);
    blit_shape_placed(t, st->screen);
    return 0;
}
