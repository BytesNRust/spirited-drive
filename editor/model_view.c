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

int model_view_render_size(int scale, int *w, int *h)
{
    const int s = scale < 1 ? 1 : scale > MODEL_RENDER_SCALE_MAX ? MODEL_RENDER_SCALE_MAX : scale;
    *w = MODEL_GAME_FRAME_W * s, *h = MODEL_GAME_FRAME_H * s;   /* the game's frame at that scale, whatever the panel's size: the panel shows it fitted */
    return s;
}

void model_view_fit(int area_w, int area_h, int w, int h, int *x, int *y, int *fw, int *fh)
{
    if (area_w < 1 || area_h < 1 || w < 1 || h < 1) {
        *x = *y = *fw = *fh = 0;
        return;
    }
    if ((long)area_w * h <= (long)area_h * w) *fw = area_w, *fh = (int)(((long)area_w * h + w / 2) / w);   /* as wide as the panel */
    else *fh = area_h, *fw = (int)(((long)area_h * w + h / 2) / h);                                      /* as tall as the panel */
    *x = (area_w - *fw) / 2, *y = (area_h - *fh) / 2;
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
