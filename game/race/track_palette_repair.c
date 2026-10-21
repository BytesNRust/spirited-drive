#include "game/race/track_palette_repair.h"

enum { STRAY = 2 };

int track_palette_repair(uint8_t pal6[TRACK_PALETTE_BYTES])
{
    uint8_t *const c = pal6 + 3 * STRAY;
    if (c[0] != 0 || c[1] != 63 || c[2] != 0) return 0;
    c[0] = 26, c[1] = 25, c[2] = 25;
    return 1;
}
