/* The race's screen palette (race_palette.h). */
#include "game/race/race_palette.h"

void race_palette_table(RacePalette *p, int first, int count, const uint8_t *rgb)
{
    const PaletteMatch m = { p->inverse, p->screen, p->enabled };
    for (int i = first; i < first + count; i++, rgb += 3) {
        const uint32_t packed = (uint32_t)rgb[2] | (uint32_t)rgb[1] << 8 | (uint32_t)rgb[0] << 16;
        const int colour = palette_match_rgb(&m, packed);
        if (i >= 0 && i < RACE_PALETTE_COLOURS)
            p->table[i] = (uint8_t)colour;
    }
}
