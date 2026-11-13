/* port: see spray_tint.h. */
#include "game/race/spray_tint.h"

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
