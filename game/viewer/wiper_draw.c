/* port (wipers): see wiper_draw.h. */
#include "viewer/wiper_draw.h"

#include <math.h>
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
                if (lx < 0 || lx >= 640 || glass[ly * 640 + lx] == 0) continue;
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

void wiper_draw(uint8_t *pixels, int32_t pitch, const int32_t box[4], const WiperPass *w, uint32_t tick, const uint8_t dark[256])
{
    if (pixels == NULL || w == NULL || dark == NULL) return;
    const float frac = wiper_fraction(tick, w->timing), k = (float)(box[3] - box[1]) / 480.0f;
    const int arm = (int)lrintf(1.4f * k), blade = (int)lrintf(3.0f * k);   /* the arm's and the blade's half widths */
    Stamper st = { pixels, pitch, box[0], box[1], box[2] - box[0], box[3] - box[1], 0, w, 0, 0, 0, 1, dark };
    st.xlim = st.w;
    st.whole = w->glass != NULL && w->glass_scale >= 1.0f && w->glass_x == floorf(w->glass_x) && w->glass_y == floorf(w->glass_y) && w->glass_scale == floorf(w->glass_scale);
    if (st.whole) st.gx = (int)w->glass_x, st.gy = (int)w->glass_y, st.gs = (int)w->glass_scale;
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
