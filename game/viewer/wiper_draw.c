/* port (wipers): see wiper_draw.h. */
#include "viewer/wiper_draw.h"

#include <math.h>
#include <stddef.h>

static void stamp(uint8_t *pixels, int32_t pitch, const int32_t box[4], const WiperPass *w, float xmax, int cx, int cy, int radius, const uint8_t dark[256])
{
    for (int y = cy - radius; y <= cy + radius; y++)
        for (int x = cx - radius; x <= cx + radius; x++) {
            if (x < 0 || y < 0 || x >= box[2] - box[0] || y >= box[3] - box[1] || (xmax > 0.0f && (float)x >= xmax)) continue;
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > radius * radius + radius) continue;
            if (w->glass != NULL) {   /* only on the glass: not over the wheel, the dash or any art */
                const int lx = (int)floorf(((float)(x) - w->glass_x) / w->glass_scale), ly = (int)floorf(((float)(y) - w->glass_y) / w->glass_scale);
                if (lx < 0 || ly < 0 || lx >= 640 || ly >= 480 || w->glass[ly * 640 + lx] == 0) continue;
            }
            uint8_t *p = pixels + (size_t)(box[1] + y) * (size_t)pitch + (size_t)(box[0] + x);
            *p = dark[*p];
        }
}

/* a line from (x0, y0) to (x1, y1) of discs `radius` wide */
static void line(uint8_t *pixels, int32_t pitch, const int32_t box[4], const WiperPass *w, float xmax, float x0, float y0, float x1, float y1, int radius, const uint8_t dark[256])
{
    const float dx = x1 - x0, dy = y1 - y0;
    const int steps = (int)ceilf(sqrtf(dx * dx + dy * dy)) + 1;
    for (int i = 0; i <= steps; i++) {
        const float t = (float)i / (float)steps;
        stamp(pixels, pitch, box, w, xmax, (int)lrintf(x0 + dx * t), (int)lrintf(y0 + dy * t), radius, dark);
    }
}

void wiper_draw(uint8_t *pixels, int32_t pitch, const int32_t box[4], const WiperPass *w, uint32_t tick, const uint8_t dark[256])
{
    if (pixels == NULL || w == NULL || dark == NULL) return;
    const float frac = wiper_fraction(tick, w->timing), k = (float)(box[3] - box[1]) / 480.0f;
    const int arm = (int)lrintf(1.4f * k), blade = (int)lrintf(3.0f * k);   /* the arm's and the blade's half widths */
    for (int i = 0; i < w->count && i < WIPER_BLADES; i++) {
        const WiperBlade *b = &w->blade[i];
        const float a = wiper_angle(b, frac);
        float tx, ty, hx, hy;
        wiper_offset(b, b->len, a, &tx, &ty);
        wiper_offset(b, b->len * 0.32f, a, &hx, &hy);
        tx += b->x, ty += b->y, hx += b->x, hy += b->y;
        line(pixels, pitch, box, w, b->xmax, b->x, b->y, tx, ty, arm < 1 ? 1 : arm, dark);
        line(pixels, pitch, box, w, b->xmax, hx, hy, tx, ty, blade < 1 ? 1 : blade, dark);
    }
}
