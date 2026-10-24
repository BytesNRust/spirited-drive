/* port: the 2D layer over the wide 3D view (layer_compose.h). */
#include "game/viewer/layer_compose.h"
#include "game/viewer/screen_layout.h"

static int in_rect(const LayerRect *r, int x, int y)
{
    return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
}

static int in_piece(const LayerCompose *c, int x, int y)
{
    for (int i = 0; i < c->piece_count; i++)
        if (in_rect(&c->pieces[i].from, x, y)) return 1;
    return 0;
}

static void put(const LayerCompose *c, uint8_t *wide, int x, int y, uint8_t p)
{
    if (p != LAYER_KEY && x >= 0 && x < c->width && y >= 0 && y < c->layer_h) wide[(int64_t)y * c->width + x] = p;
}

/* the rest of the layer: inside the car the dash (each wide pixel from the layer pixel screen_dash_x / _y give),
 * outside it centred */
static void rest(const LayerCompose *c, const uint8_t *layer, uint8_t *wide)
{
    if (!c->in_car) {
        const int dx = (c->width - c->layer_w) / 2;
        for (int y = 0; y < c->layer_h; y++)
            for (int x = 0; x < c->layer_w; x++)
                if (!in_piece(c, x, y)) put(c, wide, x + dx, y, layer[y * c->layer_w + x]);
        return;
    }
    enum { MOST_WIDTH = 4096 };
    static int column[MOST_WIDTH];
    const int width = c->width < MOST_WIDTH ? c->width : MOST_WIDTH;
    for (int x = 0; x < width; x++) column[x] = screen_dash_x(c->width, x);
    for (int y = 0; y < c->layer_h; y++) {
        const int sy = c->width > c->layer_w ? screen_dash_y(y) : y;
        if (sy < 0 || sy >= c->layer_h) continue;
        for (int x = 0; x < width; x++) {
            const int sx = column[x];
            if (sx < c->layer_w && !in_piece(c, sx, sy)) put(c, wide, x, y, layer[sy * c->layer_w + sx]);
        }
    }
}

void layer_dash_grab(const uint8_t *art, int width, int x, int y, uint8_t *out, int w, int h)
{
    for (int j = 0; j < h; j++) {
        const int sy = screen_dash_y(y + j);
        if (sy < 0 || sy >= SCREEN_HEIGHT) continue;
        for (int i = 0; i < w; i++) {
            const uint8_t p = art[sy * ORIGINAL_WIDTH + screen_dash_x(width, x + i)];
            if (p != LAYER_KEY) out[j * w + i] = p;
        }
    }
}

static void piece(const LayerCompose *c, const LayerPiece *p, const uint8_t *layer, uint8_t *wide)
{
    const int w = p->from.right - p->from.left, h = p->from.bottom - p->from.top;
    const int32_t scale = p->scaled ? c->scale : 1 << 16;
    const int out_w = p->out_w > 0 ? p->out_w : (int)((int64_t)w * scale >> 16), out_h = (int)((int64_t)h * scale >> 16);
    for (int y = 0; y < out_h; y++) {
        const int sy = p->from.top + (int)(((int64_t)y << 16) / scale);
        for (int x = 0; x < out_w; x++) {
            int sx = p->from.left + (int)(((int64_t)x << 16) / scale);
            if (sx >= p->from.right) sx = p->from.right - 1;   /* a set out_w a column past the scaled one */
            if (sx < c->layer_w && sy < c->layer_h) put(c, wide, p->x + x, p->y + y, layer[sy * c->layer_w + sx]);
        }
    }
}

void layer_compose(const LayerCompose *c, const uint8_t *layer, uint8_t *wide)
{
    rest(c, layer, wide);
    for (int i = 0; i < c->piece_count; i++) piece(c, &c->pieces[i], layer, wide);
}
