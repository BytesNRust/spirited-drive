#include "game/race/race_state.h"

#include <string.h>

void race_note(RaceState *race, int32_t level)
{
    if (race->phase != 1) return;
    race->note_level = level;
    race->note_second = race->clock / 0x3c;
    const int32_t second = race->tick / 0x3c;
    race->note_tick = race->tick;
    if (race->note_second < 0 || race->note_second >= RACE_NOTES) return;  /* the clock stays below 0x7080 */
    int32_t *note = &race->notes[race->note_second];
    if (*note >> 8 < second) *note = 0;
    if (level > (*note & 0xff)) *note = second << 8 | (level + 0x80);
}

TriNode race_node(const RaceState *race, int32_t node)
{
    TriNode n;
    if (tri_node(race->tri, (uint32_t)node & (race->tri->ring_nodes - 1u), &n) != TRI_OK) memset(&n, 0, sizeof n);
    return n;
}
