#include "game/race/ai_zones.h"

enum { ZONES = 2, ZONE_WORDS = 6 };

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
