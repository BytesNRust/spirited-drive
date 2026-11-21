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

/* «Paint: <name>» when the car is painted with a colour, «Paint: none (base texture)» otherwise. 1 when it fits, 0 when cut (the text is then still closed) */
int model_paint_caption(const char *name, int painted, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
