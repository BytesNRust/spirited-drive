/* port (wipers): see wiper_draw.h. */
#include "viewer/wiper_draw.h"

#include "viewer/layer_compose.h"

#include <math.h>
#include <stdlib.h>
#include <stddef.h>

/* what every stamp of a draw shares: the box, the buffer, the glass map's mapping as integers (the buffer's pixel to the layer's: floor((x - gx) / gs); true when they are whole numbers, as they are for every
 * scale, else the float path), the limits */
typedef struct {
    uint8_t *pixels;
    int32_t pitch;
    int bx, by, w, h, xlim;   /* the box's origin and size; x at or past xlim is not drawn */
    const WiperPass *pass;
    int whole, gx, gy, gs;
    const uint8_t *dark;
} Stamper;

static int floor_div(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

static void stamp(const Stamper *st, int cx, int cy, int radius)
{
    const int y0 = cy - radius < 0 ? 0 : cy - radius, y1 = cy + radius >= st->h ? st->h - 1 : cy + radius;
    const int x0 = cx - radius < 0 ? 0 : cx - radius, x1 = cx + radius >= st->xlim ? st->xlim - 1 : cx + radius, edge = radius * radius + radius;
    const uint8_t *const glass = st->pass->glass;
    const int gw = st->pass->glass_w > 0 ? st->pass->glass_w : 640;
    for (int y = y0; y <= y1; y++) {
        int ly = 0;
        if (glass != NULL) {
            ly = st->whole ? floor_div(y - st->gy, st->gs) : (int)floorf(((float)y - st->pass->glass_y) / st->pass->glass_scale);
            if (ly < 0 || ly >= 480) continue;
        }
        uint8_t *const row = st->pixels + (size_t)(st->by + y) * (size_t)st->pitch + (size_t)st->bx;
        for (int x = x0; x <= x1; x++) {
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > edge) continue;
            if (glass != NULL) {   /* only on the glass: not over the wheel, the dash or any art */
                const int lx = st->whole ? floor_div(x - st->gx, st->gs) : (int)floorf(((float)x - st->pass->glass_x) / st->pass->glass_scale);
                if (lx < 0 || lx >= gw) continue;
                const uint8_t *const nose = st->pass->nose;
                if (glass[ly * gw + lx] == 0 && (nose == NULL || nose[ly * gw + lx] == LAYER_KEY)) continue;   /* the nose under the glass is swept too (dash_nose.h) */
            }
            row[x] = st->dark[row[x]];
        }
    }
}

/* a line from (x0, y0) to (x1, y1) of discs `radius` wide */
static void line(const Stamper *st, float x0, float y0, float x1, float y1, int radius)
{
    const float dx = x1 - x0, dy = y1 - y0;
    const int steps = (int)ceilf(sqrtf(dx * dx + dy * dy)) + 1;
    for (int i = 0; i <= steps; i++) {
        const float t = (float)i / (float)steps;
        stamp(st, (int)lrintf(x0 + dx * t), (int)lrintf(y0 + dy * t), radius);
    }
}

/* the map's pixel under the buffer's (bx, by) of the box, by the stamper's mapping */
static int map_of(const Stamper *st, int v, int origin, float forigin)
{
    return st->whole ? floor_div(v - origin, st->gs) : (int)floorf(((float)v - forigin) / st->pass->glass_scale);
}

/* the nose (dash_nose.h) into the box where the map has it, the rows and columns of its extent only */
static void nose_paint(const Stamper *st)
{
    const WiperPass *const w = st->pass;
    const int gw = w->glass_w > 0 ? w->glass_w : 640;
    const float s = w->glass_scale;
    const int y0 = (int)floorf(w->glass_y + (float)w->nose_box[1] * s), y1 = (int)ceilf(w->glass_y + (float)w->nose_box[3] * s);
    const int x0 = (int)floorf(w->glass_x + (float)w->nose_box[0] * s), x1 = (int)ceilf(w->glass_x + (float)w->nose_box[2] * s);
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < st->h; y++) {
        const int ly = map_of(st, y, st->gy, w->glass_y);
        if (ly < 0 || ly >= 480) continue;
        const uint8_t *const from = w->nose + (size_t)ly * (size_t)gw;
        uint8_t *const row = st->pixels + (size_t)(st->by + y) * (size_t)st->pitch + (size_t)st->bx;
        for (int x = x0 < 0 ? 0 : x0; x < x1 && x < st->w; x++) {
            const int lx = map_of(st, x, st->gx, w->glass_x);
            if (lx >= 0 && lx < gw && from[lx] != LAYER_KEY) row[x] = from[lx];
        }
    }
}

void wiper_draw(uint8_t *pixels, int32_t pitch, const int32_t box[4], const WiperPass *w, uint32_t tick, const uint8_t dark[256])
{
    if (pixels == NULL || w == NULL || dark == NULL) return;
    const char *pose = getenv("TNFS_WIPER_FRACTION");   /* port (dev, env only): the blades held at this fraction (0 parked .. 1 at the top), for the captures */
    const float frac = pose != NULL && *pose != '\0' ? fminf(fmaxf(strtof(pose, NULL), 0.0f), 1.0f) : wiper_fraction(tick, w->timing), k = (float)(box[3] - box[1]) / 480.0f;
    const int arm = (int)lrintf(1.4f * k), blade = (int)lrintf(3.0f * k);   /* the arm's and the blade's half widths */
    Stamper st = { pixels, pitch, box[0], box[1], box[2] - box[0], box[3] - box[1], 0, w, 0, 0, 0, 1, dark };
    st.xlim = st.w;
    st.whole = w->glass != NULL && w->glass_scale >= 1.0f && w->glass_x == floorf(w->glass_x) && w->glass_y == floorf(w->glass_y) && w->glass_scale == floorf(w->glass_scale);
    if (st.whole) st.gx = (int)w->glass_x, st.gy = (int)w->glass_y, st.gs = (int)w->glass_scale;
    if (w->glass != NULL && w->nose != NULL) nose_paint(&st);   /* the nose first: the blades are drawn over it */
    for (int i = 0; i < w->count && i < WIPER_BLADES; i++) {
        const WiperBlade *b = &w->blade[i];
        const float a = wiper_angle(b, frac);
        float tx, ty, hx, hy;
        wiper_offset(b, b->len, a, &tx, &ty);
        wiper_offset(b, b->len * 0.32f, a, &hx, &hy);
        tx += b->x, ty += b->y, hx += b->x, hy += b->y;
        st.xlim = st.w;   /* the layer's right edge: x < xmax is the same as x < ceil(xmax) for whole x */
        if (b->xmax > 0.0f && (int)ceilf(b->xmax) < st.xlim) st.xlim = (int)ceilf(b->xmax);
        line(&st, b->x, b->y, tx, ty, arm < 1 ? 1 : arm);
        line(&st, hx, hy, tx, ty, blade < 1 ? 1 : blade);
    }
}
