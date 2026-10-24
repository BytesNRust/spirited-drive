/* port: the 2D layer over the wide 3D view (layer_compose.h). */
#include "game/viewer/layer_compose.h"
#include "game/viewer/screen_layout.h"
#include "game/sys/bands.h"

static void put(const LayerCompose *c, uint8_t *wide, int x, int y, uint8_t p)
{
    if (p != LAYER_KEY && x >= 0 && x < c->width && y >= 0 && y < c->layer_h) wide[(int64_t)y * c->width + x] = p;
}

/* the pieces' pixels of the layer, marked for the rest's loop (the pieces' rects set, used, then cleared again) */
enum { MASK_W = 640, MASK_H = 480 };
static BANDS_THREAD_LOCAL uint8_t piece_mask[MASK_W * MASK_H];   /* port (render bands): per thread */

static void mark_pieces(const LayerCompose *c, uint8_t on)
{
    for (int i = 0; i < c->piece_count; i++) {
        const LayerRect *r = &c->pieces[i].from;
        for (int y = r->top < 0 ? 0 : r->top; y < r->bottom && y < c->layer_h; y++)
            for (int x = r->left < 0 ? 0 : r->left; x < r->right && x < c->layer_w; x++) piece_mask[y * c->layer_w + x] = on;
    }
}

/* the rest of the layer: inside the car the dash (each wide pixel from the layer pixel screen_dash_x / _y give),
 * outside it centred */
static void rest(const LayerCompose *c, const uint8_t *layer, uint8_t *wide, int top, int bottom)
{
    if (c->layer_w > MASK_W || c->layer_h > MASK_H) return;
    mark_pieces(c, 1);
    if (!c->in_car) {
        const int dx = (c->width - c->layer_w) / 2;
        for (int y = top; y < bottom; y++)
            for (int x = 0; x < c->layer_w; x++)
                if (!piece_mask[y * c->layer_w + x]) put(c, wide, x + dx, y, layer[y * c->layer_w + x]);
        mark_pieces(c, 0);
        return;
    }
    const int width = c->width < SCREEN_DASH_MOST_WIDTH ? c->width : SCREEN_DASH_MOST_WIDTH;
    const int *column = screen_dash_columns(c->width);
    for (int y = top; y < bottom; y++) {
        const int sy = c->width > c->layer_w ? screen_dash_y(y) : y;
        if (sy < 0 || sy >= c->layer_h) continue;
        const uint8_t *row = layer + sy * c->layer_w, *masked = piece_mask + sy * c->layer_w;
        uint8_t *out = wide + (int64_t)y * c->width;
        for (int x = 0; x < width; x++) {
            const int sx = column[x];
            if (sx < c->layer_w && !masked[sx] && row[sx] != LAYER_KEY) out[x] = row[sx];
        }
    }
    mark_pieces(c, 0);
}

static int dash_column(int width, int x)
{
    return x >= 0 && x < width && x < SCREEN_DASH_MOST_WIDTH ? screen_dash_columns(width)[x] : screen_dash_x(width, x);
}

void layer_dash_grab(const uint8_t *art, int width, int x, int y, uint8_t *out, int w, int h)
{
    for (int j = 0; j < h; j++) {
        const int sy = screen_dash_y(y + j);
        if (sy < 0 || sy >= SCREEN_HEIGHT) continue;
        for (int i = 0; i < w; i++) {
            const uint8_t p = art[sy * ORIGINAL_WIDTH + dash_column(width, x + i)];
            if (p != LAYER_KEY) out[j * w + i] = p;
        }
    }
}

static void piece(const LayerCompose *c, const LayerPiece *p, const uint8_t *layer, uint8_t *wide, int top, int bottom)
{
    const int w = p->from.right - p->from.left, h = p->from.bottom - p->from.top;
    const int32_t scale = p->scaled ? c->scale : 1 << 16;
    const int out_w = p->out_w > 0 ? p->out_w : (int)((int64_t)w * scale >> 16), out_h = (int)((int64_t)h * scale >> 16);
    for (int y = 0; y < out_h; y++) {
        if (p->y + y < top || p->y + y >= bottom) continue;   /* port (render bands): the rows asked for */
        const int sy = p->from.top + (int)(((int64_t)y << 16) / scale);
        for (int x = 0; x < out_w; x++) {
            int sx = p->from.left + (int)(((int64_t)x << 16) / scale);
            if (sx >= p->from.right) sx = p->from.right - 1;   /* a set out_w a column past the scaled one */
            if (sx < c->layer_w && sy < c->layer_h) put(c, wide, p->x + x, p->y + y, layer[sy * c->layer_w + sx]);
        }
    }
}

void layer_compose_rows(const LayerCompose *c, const uint8_t *layer, uint8_t *wide, int top, int bottom)
{
    if (top < 0) top = 0;
    if (bottom > c->layer_h) bottom = c->layer_h;
    if (bottom <= top) return;
    rest(c, layer, wide, top, bottom);
    for (int i = 0; i < c->piece_count; i++) piece(c, &c->pieces[i], layer, wide, top, bottom);
}

void layer_compose(const LayerCompose *c, const uint8_t *layer, uint8_t *wide)
{
    layer_compose_rows(c, layer, wide, 0, c->layer_h);
}
