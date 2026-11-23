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

int model_view_pixel(int area_w, int area_h, int want)
{
    int k = want < 1 ? 1 : want > MODEL_PIXEL_MAX ? MODEL_PIXEL_MAX : want;
    while (k > 1 && (area_w / k < MODEL_PIXEL_MIN_SIDE || area_h / k < MODEL_PIXEL_MIN_SIDE)) k--;
    return k;
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
