/* The race's control panels (0044d640). */
#include "game/race/race_panel.h"

enum {
    LIVE = 1,
    DASH_KIND_LAST = 7, REPLAY_KIND_FIRST = 1, REPLAY_KIND_LAST = 5,
    KIND_SPECIAL = 13, KIND_SPECIAL_SHOWN = 3, REPLAY_FIRST_SHOWN = 1
};

static void show(int32_t *view, int32_t *redraw, int32_t kind)
{
    *view = kind;
    *redraw = 1;
}

void race_panel(const RacePanelOps *ops, RacePanelViews *v, const RacePanelInputs *in)
{
    const int32_t kind = in->chase_kind;
    if (in->unknown_4c5390 != LIVE) {
        if (kind >= 0 && kind <= DASH_KIND_LAST && kind != v->dash_view) show(&v->dash_view, &v->dash_redraw, kind);
        else if (kind == KIND_SPECIAL) show(&v->dash_view, &v->dash_redraw, KIND_SPECIAL_SHOWN);
        ops->dash(ops->ctx);
        return;
    }
    if (in->replay == 0 && in->player == in->followed && in->unknown_513e6c == 0) return;
    if (kind >= REPLAY_KIND_FIRST && kind <= REPLAY_KIND_LAST && kind != v->replay_view)
        show(&v->replay_view, &v->replay_redraw, kind);
    else if (kind == KIND_SPECIAL) show(&v->replay_view, &v->replay_redraw, KIND_SPECIAL_SHOWN);
    else if (v->replay_view == 0) show(&v->replay_view, &v->replay_redraw, REPLAY_FIRST_SHOWN);
    ops->replay(ops->ctx);
}
