/* The in-car dashboard (cockpit.h). */
#include "game/race/cockpit.h"

void cockpit_queue_palette(RacePalette *pal, const uint8_t *palette)
{
    race_palette_queue(pal, COCKPIT_COLOUR_FIRST, COCKPIT_COLOUR_COUNT, palette);
}
