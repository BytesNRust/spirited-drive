#include "model_view.h"

#include <stdio.h>

ModelView model_view_of(int card_view, int as_card)
{
    if (!card_view) return MODEL_VIEW_MODEL;
    return as_card ? MODEL_VIEW_GAME : MODEL_VIEW_CARD;
}

void model_view_flags(ModelView view, int *card_view, int *as_card)
{
    *card_view = view != MODEL_VIEW_MODEL;
    *as_card = view == MODEL_VIEW_GAME;
}

int model_view_render_size(int area_w, int area_h, int scale, int *w, int *h)
{
    const int s = scale < 1 ? 1 : scale > MODEL_RENDER_SCALE_MAX ? MODEL_RENDER_SCALE_MAX : scale;
    if (area_w < 1 || area_h < 1) return *w = *h = 0, s;
    const long frame_h = (long)MODEL_GAME_FRAME_H * s;
    const long rh = area_h < frame_h ? area_h : frame_h;   /* never finer than the panel's own pixels */
    long rw = (rh * area_w + area_h / 2) / area_h;
    if (rw < 1) rw = 1;
    *w = (int)rw, *h = (int)rh;
    return s;
}

const char *model_view_name(ModelView view)
{
    static const char *const NAMES[MODEL_VIEWS] = { "Model", "Card", "As the game draws it" };
    return view >= 0 && view < MODEL_VIEWS ? NAMES[view] : "";
}

int model_paint_caption(const char *name, int painted, char *out, size_t cap)
{
    if (cap == 0) return 0;
    const int n = painted && name != NULL ? snprintf(out, cap, "Paint: %s", name[0] != '\0' ? name : "unnamed") : snprintf(out, cap, "Paint: none (base texture)");
    return n >= 0 && (size_t)n < cap;
}
