/* port: the 2D layer over the wide 3D view (layer_compose.h). */
#include "game/viewer/layer_compose.h"
#include "game/viewer/screen_layout.h"
#include "game/sys/bands.h"

#include <stdint.h>
#include <string.h>

/* the pieces' pixels of the layer, marked for the rest's loop (the pieces' rects set, used, then cleared again) */
enum { MASK_W = 640, MASK_H = 480 };
static BANDS_THREAD_LOCAL uint8_t piece_mask[MASK_W * MASK_H];   /* port (render bands): per thread */

/* port (perf): only the layer rows [lo, hi) the band reads are marked (every band marked every piece's whole rect before), the thread's mask once in a local (a
 * thread local read per pixel is a call on macOS) and a row of it set at a time. The mask outside [lo, hi) stays 0 (every mark is cleared with the same rows). */
static void mark_pieces(const LayerCompose *c, uint8_t on, int lo, int hi)
{
    uint8_t *const mask = piece_mask;
    for (int i = 0; i < c->piece_count; i++) {
        const LayerRect *r = &c->pieces[i].from;
        const int left = r->left < 0 ? 0 : r->left, right = r->right < c->layer_w ? r->right : c->layer_w;
        const int top = r->top < lo ? lo : r->top, bottom = r->bottom < hi ? r->bottom : hi;
        if (left >= right) continue;
        for (int y = top; y < bottom; y++) memset(mask + y * c->layer_w + left, on, (size_t)(right - left));
    }
}

/* port (perf): 1 when the layer's row is nothing but LAYER_KEY, which no loop below writes a pixel of (a key pixel leaves the frame as it was): the 3D window's rows, most of a frame's */
static int row_is_key(const uint8_t *row, int n)
{
    int i = 0;
    for (; i + 8 <= n; i += 8) {
        uint64_t v;
        memcpy(&v, row + i, 8);
        if (v != 0xffffffffffffffffull) return 0;
    }
    for (; i < n; i++)
        if (row[i] != LAYER_KEY) return 0;
    return 1;
}

/* the rest of the layer: inside the car the dash (each wide pixel from the layer pixel screen_dash_x / _y give),
 * outside it centred */
static void rest(const LayerCompose *c, const uint8_t *layer, uint8_t *wide, int top, int bottom)
{
    if (c->layer_w > MASK_W || c->layer_h > MASK_H) return;
    uint8_t *const mask = piece_mask;
    if (!c->in_car) {
        const int lo = top < 0 ? 0 : top, hi = bottom < c->layer_h ? bottom : c->layer_h;   /* the layer rows this band reads: its own */
        mark_pieces(c, 1, lo, hi);
        const int dx = (c->width - c->layer_w) / 2;
        const int x0 = dx < 0 ? -dx : 0, x1 = c->width - dx < c->layer_w ? c->width - dx : c->layer_w;   /* the columns that land on the frame */
        for (int y = top; y < bottom; y++) {
            const uint8_t *row = layer + y * c->layer_w, *masked = mask + y * c->layer_w;
            if (y < 0 || y >= c->layer_h || row_is_key(row, c->layer_w)) continue;
            uint8_t *out = wide + (int64_t)y * c->width + dx;
            for (int x = x0; x < x1; x++)
                if (!masked[x] && row[x] != LAYER_KEY) out[x] = row[x];
        }
        mark_pieces(c, 0, lo, hi);
        return;
    }
    /* the dash: the band's rows are the layer rows screen_dash_y gives (it never goes back as y goes down, so its ends are the range's) when the frame is wider than the layer */
    int first = top, last = bottom - 1;
    if (c->width > c->layer_w && bottom > top) first = screen_dash_y(top), last = screen_dash_y(bottom - 1);
    const int lo = first < 0 ? 0 : first, hi = last + 1 < c->layer_h ? last + 1 : c->layer_h;
    mark_pieces(c, 1, lo, hi);
    const int width = c->width < SCREEN_DASH_MOST_WIDTH ? c->width : SCREEN_DASH_MOST_WIDTH;
    const int *column = screen_dash_columns(c->width);
    for (int y = top; y < bottom; y++) {
        const int sy = c->width > c->layer_w ? screen_dash_y(y) : y;
        if (sy < 0 || sy >= c->layer_h) continue;
        const uint8_t *row = layer + sy * c->layer_w, *masked = mask + sy * c->layer_w;
        if (row_is_key(row, c->layer_w)) continue;
        uint8_t *out = wide + (int64_t)y * c->width;
        for (int x = 0; x < width; x++) {
            const int sx = column[x];
            if (sx < c->layer_w && !masked[sx] && row[sx] != LAYER_KEY) out[x] = row[sx];
        }
    }
    mark_pieces(c, 0, lo, hi);
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

/* port (perf): the source column of each output column x of a piece, made once for the piece and not by a division at every pixel; PIECE_COLUMNS_MAX covers any frame the port makes, and a
 * wider piece falls back to the division */
enum { PIECE_COLUMNS_MAX = 4096 };

static void piece(const LayerCompose *c, const LayerPiece *p, const uint8_t *layer, uint8_t *wide, int top, int bottom)
{
    const int w = p->from.right - p->from.left, h = p->from.bottom - p->from.top;
    const int32_t scale = p->scaled ? c->scale : 1 << 16;
    const int out_w = p->out_w > 0 ? p->out_w : (int)((int64_t)w * scale >> 16), out_h = (int)((int64_t)h * scale >> 16);
    int columns[PIECE_COLUMNS_MAX];
    const int mapped = out_w <= PIECE_COLUMNS_MAX;
    if (mapped)
        for (int x = 0; x < out_w; x++) {
            int sx = p->from.left + (int)(((int64_t)x << 16) / scale);
            if (sx >= p->from.right) sx = p->from.right - 1;   /* a set out_w a column past the scaled one */
            columns[x] = sx;
        }
    const int y0 = top - p->y > 0 ? top - p->y : 0, y1 = bottom - p->y < out_h ? bottom - p->y : out_h;   /* port (render bands): the rows asked for */
    const int x0 = p->x < 0 ? -p->x : 0, x1 = c->width - p->x < out_w ? c->width - p->x : out_w;   /* the columns that land on the frame */
    for (int y = y0; y < y1; y++) {
        const int sy = p->from.top + (int)(((int64_t)y << 16) / scale), yy = p->y + y;
        if (sy >= c->layer_h || yy < 0 || yy >= c->layer_h) continue;
        uint8_t *out = wide + (int64_t)yy * c->width + p->x;
        for (int x = x0; x < x1; x++) {
            int sx;
            if (mapped) sx = columns[x];
            else {
                sx = p->from.left + (int)(((int64_t)x << 16) / scale);
                if (sx >= p->from.right) sx = p->from.right - 1;
            }
            if (sx < c->layer_w) {
                const uint8_t v = layer[sy * c->layer_w + sx];
                if (v != LAYER_KEY) out[x] = v;
            }
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

/* n source pixels as `s`-pixel runs: the same bytes as s memsets of s bytes each, without a call per pixel */
static void expand_row(uint8_t *dst, const uint8_t *src, int n, int s)
{
    if (s == 2) {
        for (int i = 0; i < n; i++) {
            const uint16_t v = (uint16_t)(src[i] * 0x0101);
            memcpy(dst + 2 * i, &v, 2);
        }
    } else if (s == 3) {
        for (int i = 0; i < n; i++) dst[3 * i] = dst[3 * i + 1] = dst[3 * i + 2] = src[i];
    } else {
        for (int i = 0; i < n; i++) memset(dst + (size_t)i * s, src[i], (size_t)s);
    }
}

/* each run of drawn pixels is expanded once into its block's first row and copied down into the others; the 3D's
 * (LAYER_KEY) stretches, the most of a frame, are skipped eight pixels at a time */
void layer_upscale_rows(const uint8_t *layer, int width, int scale, uint8_t *pixels, int top, int bottom)
{
    const size_t out_w = (size_t)width * scale;
    for (int y = top; y < bottom; y++) {
        const uint8_t *src = layer + (size_t)y * width;
        for (int x = 0; x < width;) {
            for (uint64_t eight; x + 8 <= width && (memcpy(&eight, src + x, 8), eight == UINT64_MAX);) x += 8;
            while (x < width && src[x] == LAYER_KEY) x++;
            const int first = x;
            while (x < width && src[x] != LAYER_KEY) x++;
            if (x == first) continue;
            uint8_t *block = pixels + (size_t)y * scale * out_w + (size_t)first * scale;
            expand_row(block, src + first, x - first, scale);
            for (int j = 1; j < scale; j++) memcpy(block + (size_t)j * out_w, block, (size_t)(x - first) * scale);
        }
    }
}
