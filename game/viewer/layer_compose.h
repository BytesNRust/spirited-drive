/* port: the wide screen's frame. Everything the original draws in 2D (the dashboard, the HUD, the map, the mirror,
 * the panels) is drawn as it always was into a 640 x 480 layer first filled with LAYER_KEY; the 3D view alone is
 * drawn at the wide width. layer_compose puts the layer over the view: inside the car the dash at its size with its
 * sides stretched to the width (screen_dash_x / _y), outside it centred, and the pieces (the HUD's groups, the mirror, the map, the
 * radar, the gauge) moved to their anchors. */
#ifndef TNFS_LAYER_COMPOSE_H
#define TNFS_LAYER_COMPOSE_H

#include <stdint.h>

enum { LAYER_KEY = 0xff };      /* nothing drawn: 0xff is out of the race's colours (palette_usable) */

typedef struct {
    int left, top, right, bottom;   /* in the layer */
} LayerRect;

typedef struct {
    LayerRect from;
    int x, y;                       /* its top left on the wide frame */
    int scaled;                     /* drawn enlarged by the compose's scale (the mirror) */
    int out_w;                      /* the width drawn, 0 `from`'s scaled: a piece whose end must meet another's */
} LayerPiece;

typedef struct {
    int layer_w, layer_h;           /* 640, 480 */
    int width;                      /* the wide frame's, its height the layer's */
    int32_t scale;                  /* 16.16: the scaled pieces' enlargement (the mirror, width / 640) */
    int in_car;                     /* 1: the rest of the layer is the dash (screen_dash_x / _y); 0: the rest centred */
    const LayerPiece *pieces;
    int piece_count;
} LayerCompose;

void layer_compose(const LayerCompose *c, const uint8_t *layer, uint8_t *wide);

/* port (render bands): the wide frame's rows [top, bottom) of layer_compose, nothing else written: each row is the
 * same as layer_compose gives it, so bands of rows on several threads (any order) make the whole. Thread safe for
 * disjoint rows (the pieces' mask is per thread). */
void layer_compose_rows(const LayerCompose *c, const uint8_t *layer, uint8_t *wide, int top, int bottom);

/* port: `out` (w x h) as the in-car wide frame `width` wide shows the dash's art (640 x 480) from (x, y): each pixel
 * from the art's pixel screen_dash_x / _y give, the art's LAYER_KEY (the windshield) leaving `out` as it was */
void layer_dash_grab(const uint8_t *art, int width, int x, int y, uint8_t *out, int w, int h);

#endif
