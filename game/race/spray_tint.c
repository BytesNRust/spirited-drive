/* port (rally_spray): see spray_tint.h. */
#include "game/race/spray_tint.h"

static int luma_of(int r, int g, int b) { return (r * 77 + g * 150 + b * 29) >> 8; }

enum { SNOW_LUMA = 190, DIRTY_PERCENT = 62 };   /* a road lighter than this is snow; the others' mist is this much of the road's brightness */

void spray_tint_target(const uint8_t road[3], uint8_t out[3])
{
    const int l = luma_of(road[0], road[1], road[2]);
    for (int k = 0; k < 3; k++) {
        if (l >= SNOW_LUMA) out[k] = (uint8_t)(road[k] + (255 - road[k]) / 3);   /* white powder, a little lighter than the snow it flies from */
        else out[k] = (uint8_t)(road[k] * DIRTY_PERCENT / 100);
    }
}

void spray_tint_table(const uint8_t pal[768], const uint8_t tint[3], int percent, uint8_t table[256])
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    for (int i = 0; i < 256; i++) {
        table[i] = (uint8_t)i;
        if (i == 0xFF) continue;
        int want[3];
        for (int k = 0; k < 3; k++) want[k] = (pal[i * 3 + k] * (100 - percent) + tint[k] * percent + 50) / 100;
        int best = i, best_d = 1 << 30;
        for (int j = 0; j < 255; j++) {   /* the nearest palette entry; the lower index wins a tie but the entry itself wins its own colour */
            const int dr = pal[j * 3] - want[0], dg = pal[j * 3 + 1] - want[1], db = pal[j * 3 + 2] - want[2];
            const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;   /* the eye's weights, roughly */
            if (d < best_d || (d == best_d && j == i)) best_d = d, best = j;
        }
        table[i] = (uint8_t)best;
    }
}

uint32_t spray_tint_key(const uint8_t colour[3])
{
    return (uint32_t)(colour[0] >> 4) << 8 | (uint32_t)(colour[1] >> 4) << 4 | (uint32_t)(colour[2] >> 4);
}
