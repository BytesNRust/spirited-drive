/* The Model window's View choice and its caption (items 7 and 8 of the owner's review). The old Card view and As on the card boxes were two toggles that
 * sound the same: they are one choice, Model / Card / As the game draws it (the window still keeps the two flags, the steps and the card photo use them).
 * The caption under the picture names the colour the car is painted with. No window, no ImGui. */
#ifndef TNFS_TRACK_EDITOR_MODEL_VIEW_H
#define TNFS_TRACK_EDITOR_MODEL_VIEW_H

#include <stddef.h>
#include <stdint.h>

typedef enum { MODEL_VIEW_MODEL = 0, MODEL_VIEW_CARD, MODEL_VIEW_GAME, MODEL_VIEWS } ModelView;

#ifdef __cplusplus
extern "C" {
#endif

/* the choice the two flags stand for (As on the card without Card view is the Model: nothing of it shows) */
ModelView model_view_of(int card_view, int as_card);
/* the two flags of a choice */
void model_view_flags(ModelView view, int *card_view, int *as_card);
const char *model_view_name(ModelView view);

/* The Model's render scale (the game's own setting, 1..MODEL_RENDER_SCALE_MAX): the car is drawn on the game's 4:3 frame at that scale (640 x 480 times it: 1280 x 960 at 2),
 * whatever the panel's size, and the panel shows that frame fitted: as coarse as the game's own picture at that scale, its dither in the buffer as the game has it. Fills *w and *h and returns the scale used (the one asked for, kept to 1..MODEL_RENDER_SCALE_MAX). */
enum { MODEL_RENDER_SCALE_MAX = 3, MODEL_GAME_FRAME_W = 640, MODEL_GAME_FRAME_H = 480 };
int model_view_render_size(int scale, int *w, int *h);
/* The Model view's picture: the panel's shape, not the game's 4:3 (the Card and As the game draws it views keep 4:3). Its height is the game's frame's at the render scale (480 times it) and its width follows the
 * panel's (area_w : area_h), so the car fills the whole panel with no bars at the sides; a picture wider than max_w is made smaller with the same shape. Fills *w and *h (both at least 1) and returns 1, or 0 for an
 * area with no size. */
int model_view_frame_size(int scale, int area_w, int area_h, int max_w, int *w, int *h);
/* How far (in the picture's pixels, *sx and *sy) a w x h picture is to be moved so that the box the car is drawn in (x0, y0, x1, y1: box[0..3], x1 > x0) is in the middle of it; 0 and 0 for an empty box. A car whole in the
 * picture stays whole in it after the move (the box centred cannot leave it). */
void model_view_centre_shift(int w, int h, const int box[4], int *sx, int *sy);
/* a w x h picture fitted into an area_w x area_h panel with its shape kept, in the middle: its place (*x, *y) in the area and its size (all 0 for an empty one) */
void model_view_fit(int area_w, int area_h, int w, int h, int *x, int *y, int *fw, int *fh);

/* The Color box's drag changes a paint on every frame: the scene is made again at most every MODEL_COLOUR_REBUILD_MS (10 a second, the last change taken when the drag stops), and the picture
 * stays the scale 1 frame for MODEL_COLOUR_LOW_MS after the last one, the full frame once it has stopped. Clocks in ms; `last` is 0 when no scene was made again for a colour yet. */
enum { MODEL_COLOUR_REBUILD_MS = 100, MODEL_COLOUR_LOW_MS = 300 };
int model_colour_rebuild_due(uint32_t now, uint32_t last);   /* 1 when a scene made again now is allowed */
int model_colour_low(uint32_t now, uint32_t last);           /* 1 when the picture is still the scale 1 one */

/* «Paint: <name>» when the car is painted with a colour, «Paint: none (base texture)» otherwise. 1 when it fits, 0 when cut (the text is then still closed) */
int model_paint_caption(const char *name, int painted, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
