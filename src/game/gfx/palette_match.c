/* RGB to palette index, 8-bit path. Ported from NFS_WIN.EXE 00473140; the
 * address of each piece is in palette_match.h. */
#include "game/gfx/palette_match.h"

#include <stddef.h>

enum {
    BEST_START = 99999,  /* the search's initial "best" distance */
    CHANNEL_MASK = 0xFF
};

uint8_t palette_match_inverse(const uint8_t inverse[PALETTE_MATCH_INVERSE_SIZE], uint32_t rgb)
{
    const uint32_t index = ((rgb >> 19) & 0x1F) << 10 | ((rgb >> 11) & 0x1F) << 5 | ((rgb >> 3) & 0x1F);
    return inverse[index];
}

int palette_match_nearest(const uint8_t palette[PALETTE_MATCH_ENTRIES * PALETTE_MATCH_ENTRY_BYTES],
                          const uint8_t enabled[PALETTE_MATCH_ENTRIES], uint32_t rgb)
{
    const int32_t c0 = (int32_t)(rgb & CHANNEL_MASK);
    const int32_t c1 = (int32_t)((rgb >> 8) & CHANNEL_MASK);
    const int32_t c2 = (int32_t)((rgb >> 16) & CHANNEL_MASK);
    int32_t best = BEST_START;
    int best_index = 0;
    for (int i = 0; i < PALETTE_MATCH_ENTRIES; i++) {
        if (enabled[i] == 0) continue;
        const uint8_t *entry = palette + (size_t)i * PALETTE_MATCH_ENTRY_BYTES;
        const int32_t d0 = c0 - entry[0], d1 = c1 - entry[1], d2 = c2 - entry[2];
        const int32_t distance = d0 * d0 + d1 * d1 + d2 * d2;
        if (distance < best) {
            best = distance;
            best_index = i;
        }
    }
    return best_index;
}

int palette_match_rgb(const PaletteMatch *m, uint32_t rgb)
{
    if (m == NULL) return 0;
    if (m->inverse != NULL) return palette_match_inverse(m->inverse, rgb);
    if (m->palette == NULL || m->enabled == NULL) return 0;
    return palette_match_nearest(m->palette, m->enabled, rgb);
}
