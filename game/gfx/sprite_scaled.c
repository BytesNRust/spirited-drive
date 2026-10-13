/* A shape drawn scaled and blended (004900a8, 0048cae9). */
#include "game/gfx/sprite_scaled.h"

#include "game/gfx/x86_int.h"

#include <stddef.h>

enum { SHAPE_8_BIT = 0x7b, SHAPE_PIXELS = 0x10, MAX_ROW = 4096 };

static int32_t half(int32_t v)
{
    return x_sar(x_add(v, v < 0 ? 1 : 0), 1);
}

static int32_t magnitude(int32_t v)
{
    return v > 0 ? v : x_sub(0, v);
}

int sprite_scaled_blend(const QuadTarget *t, const uint8_t *const blend[256], const uint8_t *shape, int32_t w, int32_t h,
                        int32_t x, int32_t y)
{
    const int32_t width = (int16_t)(shape[4] | shape[5] << 8), height = (int16_t)(shape[6] | shape[7] << 8);
    if (w == 0 || h == 0) return 0;
    if (shape[0] != SHAPE_8_BIT) return -1;
    if (x >= t->clip_right || y >= t->clip_bottom) return 0;
    if (x_add(magnitude(w), x) <= t->clip_left || x_add(magnitude(h), y) <= t->clip_top) return 0;
    const int32_t du = x_div(x_shl(width, 16), w), dv = x_div(x_shl(height, 16), h);
    int32_t v = half(dv), u = half(du);
    if (h < 0) h = x_sub(0, h), v = x_add(v, x_shl(height, 16));
    if (w < 0) w = x_sub(0, w), u = x_add(u, x_shl(width, 16));
    if (y < t->clip_top) {
        const int32_t d = x_sub(t->clip_top, y);
        v = x_add(v, x_mul(dv, d)), h = x_sub(h, d), y = t->clip_top;
    }
    if (x_add(y, h) > t->clip_bottom) h = x_sub(t->clip_bottom, y);
    if (x < t->clip_left) {
        const int32_t d = x_sub(t->clip_left, x);
        u = x_add(u, x_mul(du, d)), w = x_sub(w, d), x = t->clip_left;
    }
    if (x_add(x, w) > t->clip_right) w = x_sub(t->clip_right, x);
    if (w > MAX_ROW) return 0;  /* port guard: the clip is narrower */
    int32_t offsets[MAX_ROW];
    for (int32_t k = 0; k < w; k++) offsets[k] = x_sar(x_add(u, x_mul(du, k)), 16);
    uint8_t *row = t->pixels + (ptrdiff_t)y * t->pitch + x;
    for (int32_t r = 0; r < h; r++, row += t->pitch, v = x_add(v, dv)) {
        const uint8_t *texels = shape + SHAPE_PIXELS + x_mul(x_sar(v, 16), width);
        for (int32_t k = 0; k < w; k++) row[k] = blend[texels[offsets[k]]][row[k]];
    }
    return 0;
}
