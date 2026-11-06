#include "game/viewer/shade_fit.h"

static int nearest_usable(const uint8_t *rgbx, const uint8_t hidden[256], const uint8_t enabled[256], int target, int fallback)
{
    int best = fallback, best_d = 1 << 30;
    for (int e = 0; e < 256; e++) {
        if (!enabled[e] || hidden[e]) continue;
        int d = 0;
        for (int c = 0; c < 3; c++) {
            const int diff = rgbx[4 * e + c] - rgbx[4 * target + c];
            d += diff * diff;
        }
        if (d < best_d) best_d = d, best = e;
    }
    return best;
}

int shade_refit(uint8_t shade[256], const uint8_t *before_rgbx, const uint8_t hidden[256], const uint8_t enabled[256])
{
    int changed = 0;
    for (int i = 0; i < 256; i++) {
        if (hidden[i] || !hidden[shade[i]]) continue;
        const int to = nearest_usable(before_rgbx, hidden, enabled, shade[i], shade[i]);
        if (to != shade[i]) shade[i] = (uint8_t)to, changed++;
    }
    return changed;
}
