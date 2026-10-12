/* The race screen's keys (00420710). */
#include "game/race/hud_keys.h"

static int parts_held(const HudKeysInputs *in)
{
    if (in->keys[in->parts_key] != 0) return 1;
    return in->pad != 0 && in->pad_button[0] != 3 && in->pad_button[1] != 3 && (in->pad_flags & 2) != 0;
}

static void layout_key(const HudKeysOps *ops, HudState *hud, const RaceState *race)
{
    if (race->track == 3 && race->race_type == 0) hud->mode = (hud->mode + 1) % 3;
    else hud->mode = 1 - hud->mode;
    hud->redraw = 1;
    ops->labels(ops->ctx);
}

static void parts_key(const HudKeysOps *ops, HudState *hud, HudKeysState *st, const RaceState *race,
                      const HudKeysInputs *in)
{
    st->use_parts = 1 - st->use_parts;
    if (st->use_parts != 0) return;
    hud->unknown_4f9a20[1] = 1;
    if (race->chase.kind != 0 || st->dash_camera != 0) ops->parts_redraw(ops->ctx);
    if (in->screen_kind == 3) st->clear_view = 1;
}

void hud_keys(const HudKeysOps *ops, HudState *hud, HudKeysState *st, RaceState *race, const HudKeysInputs *in)
{
    if (hud->lights_until < race->tick) {
        ops->labels(ops->ctx);
        hud->waiting = 0;
    }
    if (in->keys[HUD_KEY_LAYOUT] != 0) {
        if (st->armed == 0) return;
        layout_key(ops, hud, race);
        st->armed = 0;
        return;
    }
    if (in->keys[HUD_KEY_REDRAW] != 0) {
        if (st->armed == 0) return;
        hud->redraw = 1 - hud->redraw;
        ops->labels(ops->ctx);
        st->armed = 0;
        return;
    }
    if (parts_held(in)) {
        if (st->armed == 0) return;
        parts_key(ops, hud, st, race, in);
        st->armed = 0;
        return;
    }
    if (in->keys[HUD_KEY_CAMERA] == 0 || race->replay != 0 || race->unknown_513e6c != 0 || race->chase.kind > 1) {
        st->armed = 1;
        return;
    }
    if (st->armed == 0) return;
    st->dash_camera = st->dash_camera == 0;
    race->chase_kinds[0].camera = st->dash_camera;
    ops->take(ops->ctx, race->chase.kind);
    st->view_changed = 999;
    st->armed = 0;
}
