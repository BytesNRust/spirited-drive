/* The Model window's View choice and its caption (items 7 and 8 of the owner's review). The old Card view and As on the card boxes were two toggles that
 * sound the same: they are one choice, Model / Card / As the game draws it (the window still keeps the two flags, the steps and the card photo use them).
 * The caption under the picture names the colour the car is painted with. No window, no ImGui. */
#ifndef TNFS_TRACK_EDITOR_MODEL_VIEW_H
#define TNFS_TRACK_EDITOR_MODEL_VIEW_H

#include <stddef.h>

typedef enum { MODEL_VIEW_MODEL = 0, MODEL_VIEW_CARD, MODEL_VIEW_GAME, MODEL_VIEWS } ModelView;

#ifdef __cplusplus
extern "C" {
#endif

/* the choice the two flags stand for (As on the card without Card view is the Model: nothing of it shows) */
ModelView model_view_of(int card_view, int as_card);
/* the two flags of a choice */
void model_view_flags(ModelView view, int *card_view, int *as_card);
const char *model_view_name(ModelView view);

/* The Model's render scale (the game's own setting, 1..MODEL_RENDER_SCALE_MAX): the car is drawn on a frame MODEL_GAME_FRAME_H * scale tall (the game's 480 lines at scale 1, 960 at 2) in the area's shape and
 * shown stretched to the whole area, as coarse as the game's picture at that scale. Never finer than the area's own pixels (a small panel is drawn at its size). Fills *w and *h (0 for an empty
 * area) and returns the scale used (the one asked for, kept to 1..MODEL_RENDER_SCALE_MAX). */
enum { MODEL_RENDER_SCALE_MAX = 3, MODEL_GAME_FRAME_H = 480 };
int model_view_render_size(int area_w, int area_h, int scale, int *w, int *h);

/* «Paint: <name>» when the car is painted with a colour, «Paint: none (base texture)» otherwise. 1 when it fits, 0 when cut (the text is then still closed) */
int model_paint_caption(const char *name, int painted, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
