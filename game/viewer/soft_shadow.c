#include "game/viewer/soft_shadow.h"

#include "game/gfx/flat.h"

enum { BRIGHT_FROM = 192, MID_FROM = 96, RIM = 2 };

SoftShadowLevel soft_shadow_level(uint8_t light)
{
    return light >= BRIGHT_FROM ? SOFT_SHADOW_BRIGHT : light >= MID_FROM ? SOFT_SHADOW_MID : SOFT_SHADOW_DARK;
}

void soft_shadow_half(const uint8_t palette[768], const uint8_t full[256], uint8_t half[256])
{
    for (int i = 0; i < 256; i++) {
        int want[3], best = i, best_d = 1 << 30;
        for (int c = 0; c < 3; c++) want[c] = (palette[3 * i + c] + palette[3 * full[i] + c]) / 2;
        for (int e = 0; e < 256; e++) {
            int d = 0;
            for (int c = 0; c < 3; c++) d += (palette[3 * e + c] - want[c]) * (palette[3 * e + c] - want[c]);
            if (d < best_d) best = e, best_d = d;
        }
        half[i] = (uint8_t)best;
    }
}

void soft_shadow_shrink(const int32_t in[8], int32_t out[8], int px)
{
    int32_t cx = 0, cy = 0;
    for (int k = 0; k < 4; k++) cx += in[2 * k], cy += in[2 * k + 1];
    cx /= 4, cy /= 4;
    for (int k = 0; k < 4; k++) {
        const int32_t dx = in[2 * k] - cx, dy = in[2 * k + 1] - cy;
        const int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        out[2 * k] = cx + (dx < 0 ? -1 : 1) * (ax > px ? ax - px : 0);
        out[2 * k + 1] = cy + (dy < 0 ? -1 : 1) * (ay > px ? ay - px : 0);
    }
}

void soft_shadow_draw(const QuadTarget *target, const uint8_t full[256], const uint8_t half[256], SoftShadowLevel level, const int32_t screen[8])
{
    int32_t core[8];
    soft_shadow_shrink(screen, core, RIM);
    if (level == SOFT_SHADOW_MID) {
        flat_polygon_shade(target, half, 4, screen, 0);
    } else if (level == SOFT_SHADOW_DARK) {
        flat_polygon_shade(target, half, 4, core, 0);
    } else {   /* the rim as four strips round the core, so that no texel is darkened twice */
        for (int k = 0; k < 4; k++) {
            const int n = (k + 1) % 4;
            const int32_t strip[8] = { screen[2 * k], screen[2 * k + 1], screen[2 * n], screen[2 * n + 1], core[2 * n], core[2 * n + 1], core[2 * k], core[2 * k + 1] };
            flat_polygon_shade(target, half, 4, strip, 0);
        }
        int32_t inside[8];   /* the strips take their inner edge too: the core starts a texel in, or that edge would be darkened twice */
        soft_shadow_shrink(core, inside, 1);
        flat_polygon_shade(target, full, 4, inside, 0);
    }
}
