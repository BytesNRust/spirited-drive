#include "game/race/reflector.h"

#include <math.h>
#include <string.h>

static double smooth(double t)
{
    t = t < 0.0 ? 0.0 : t > 1.0 ? 1.0 : t;
    return t * t * (3.0 - 2.0 * t);
}

int reflector_gain(double forward_m, double lateral_m, double height_m, int dark)
{
    if (!dark || forward_m <= 0.0 || forward_m >= REFLECTOR_REACH_M) return 0;
    const double reach = REFLECTOR_REACH_M, near = REFLECTOR_NEAR_M;
    const double d = forward_m <= near ? 1.0 : smooth(1.0 - (forward_m - near) / (reach - near));
    const double side = 1.0 - fabs(lateral_m) / (forward_m * REFLECTOR_HALF_WIDTH_PERCENT / 100.0 + 1.0);   /* +1 m: the width of the car itself */
    const double up = (height_m - REFLECTOR_EYE_CM / 100.0) / forward_m;
    const double above = smooth(1.0 - (up * 100.0 - REFLECTOR_UP_FULL_PERCENT) / (REFLECTOR_UP_END_PERCENT - REFLECTOR_UP_FULL_PERCENT));
    const double g = d * smooth(side) * (up <= 0.0 ? 1.0 : above);
    const int v = (int)(g * 256.0 + 0.5);
    return v < 0 ? 0 : v > 256 ? 256 : v;
}

int reflector_level(int gain)
{
    if (gain <= 0) return 0;
    const int l = 1 + (gain > 256 ? 256 : gain) * (REFLECTOR_LEVELS - 1) / 257;
    return l >= REFLECTOR_LEVELS ? REFLECTOR_LEVELS - 1 : l;
}

size_t reflector_mask_scale(const uint8_t *src, int sw, int sh, uint8_t *dst, int dw, int dh)
{
    if (src == NULL || dst == NULL || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return 0;
    memset(dst, 0, (size_t)dw * (size_t)dh);
    for (int y = 0; y < sh; y++) {
        const int ty = (int)((long long)y * dh / sh);
        for (int x = 0; x < sw; x++) {
            if (!src[(size_t)y * (size_t)sw + (size_t)x]) continue;
            dst[(size_t)ty * (size_t)dw + (size_t)((long long)x * dw / sw)] = 1;
        }
    }
    size_t n = 0;
    for (size_t i = 0; i < (size_t)dw * (size_t)dh; i++) n += dst[i];
    return n;
}

int reflector_colours(const uint8_t *pixels, const uint8_t *mask, size_t count, uint8_t *out, int max)
{
    size_t use[256] = { 0 };
    for (size_t i = 0; i < count; i++)
        if (mask[i]) use[pixels[i]]++;
    int n = 0;
    while (n < max) {
        int best = -1;
        for (int c = 0; c < 256; c++)
            if (use[c] > 0 && (best < 0 || use[c] > use[best])) best = c;
        if (best < 0) break;
        out[n++] = (uint8_t)best, use[best] = 0;
    }
    return n;
}

size_t reflector_paint(uint8_t *pixels, const uint8_t *mask, size_t count, const uint8_t entry[256])
{
    size_t n = 0;
    for (size_t i = 0; i < count; i++) {
        if (!mask[i] || entry[pixels[i]] == pixels[i]) continue;
        pixels[i] = entry[pixels[i]], n++;
    }
    return n;
}
