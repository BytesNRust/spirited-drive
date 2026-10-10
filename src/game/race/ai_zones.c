#include "game/race/ai_zones.h"

enum { ZONES = 2, ZONE_WORDS = 6, NOTE_ZONE_MOVED = 0x50 };

static Car *slot_car(const RaceState *race, int32_t slot)
{
    return (uint32_t)slot < RACE_PLAYERS ? race->cars[slot] : NULL;  /* port guard */
}

void ai_swap_zones(RaceState *race, int32_t slot, int32_t zone, int32_t old)
{
    if ((uint32_t)zone >= ZONES || (uint32_t)old >= ZONES) return;  /* port guard */
    const int32_t holder = race->ai.zones[zone * ZONE_WORDS];
    Car *const held = slot_car(race, holder);
    if (held != NULL) held->zone = old;
    race->ai.zones[old * ZONE_WORDS] = (int16_t)holder;
    Car *const car = slot_car(race, slot);
    if (car != NULL) car->zone = zone;
    race->ai.zones[zone * ZONE_WORDS] = (int16_t)slot;
    for (int32_t i = race->racers; i < race->car_count; i++) {
        Car *const other = slot_car(race, i);
        if (other == NULL) continue;
        if (other->zone == zone) other->zone = old;
        else if (other->zone == old) other->zone = zone;
    }
}

void ai_zones_by_place(RaceState *race)
{
    int32_t zone = 0;
    for (int32_t i = race->car_count - 1; i >= 0; i--) {
        const Car *const car = i < RACE_PLAYERS ? slot_car(race, race->by_progress[i]) : NULL;  /* port guard */
        if (car == NULL) continue;
        const int32_t slot = car->index;
        if (slot < 0 || slot >= race->racers) continue;
        if (car->zone != zone) {
            ai_swap_zones(race, slot, zone, car->zone);
            race_note(race, NOTE_ZONE_MOVED);  /* the original checks +0x516 again; the swap leaves it */
        }
        zone++;
    }
}
