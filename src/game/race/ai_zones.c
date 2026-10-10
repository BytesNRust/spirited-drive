#include "game/race/ai_zones.h"

enum {
    ZONES = 2,
    ZONE_WORDS = 6,
    NOTE_ZONE_MOVED = 0x50,
    /* a zone record's words */
    Z_SLOT = 0, Z_KEEP_END = 1, Z_KEEP_START = 2, Z_AT = 3, Z_SEEN_END = 4, Z_SEEN_START = 5,
    SEEN_AHEAD = 0x2b,
    SEEN_BEHIND = 0xc,
    KEEP_AHEAD = 0xb,   /* past the seen end */
    KEEP_BEHIND = 8     /* inside the seen start */
};

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

static int16_t *zone_words(RaceState *race, int32_t zone)
{
    return &race->ai.zones[zone * ZONE_WORDS];
}

/* Seen spans that cross: the gap is shared out, the front zone's start
 * moved back and the one behind it's end moved forward. */
static void split_crossing(RaceState *race)
{
    int32_t gap = 0, last = 0;
    for (int32_t next = 1; next < ZONES; next++) {
        int16_t *const back = zone_words(race, last), *const front = zone_words(race, next);
        if (back[Z_SEEN_START] >= front[Z_SEEN_END]) break;
        back[Z_SEEN_START] = (int16_t)(back[Z_SEEN_START] + gap / 2);
        gap += front[Z_SEEN_END] - back[Z_SEEN_START];
        front[Z_SEEN_START] = (int16_t)(front[Z_SEEN_START] - gap / 2);
        last = next;
    }
    int16_t *const first = zone_words(race, 0);
    first[Z_SEEN_END] = (int16_t)(first[Z_SEEN_END] + gap / 2);
}

void ai_zone_spans(RaceState *race)
{
    if (race->racers > 2 || race->racers == 1) return;
    ai_zones_by_place(race);
    for (int32_t k = 0; k < ZONES; k++) {
        int16_t *const z = zone_words(race, k);
        const Car *const car = slot_car(race, z[Z_SLOT]);
        const int32_t at = car != NULL ? car->progress : 0;  /* port guard */
        z[Z_AT] = (int16_t)at;
        z[Z_SEEN_END] = (int16_t)(at + SEEN_AHEAD);
        z[Z_SEEN_START] = (int16_t)(at - SEEN_BEHIND);
    }
    split_crossing(race);
    for (int32_t k = 0; k < ZONES; k++) {
        int16_t *const z = zone_words(race, k);
        z[Z_KEEP_END] = (int16_t)(z[Z_SEEN_END] + KEEP_AHEAD);
        z[Z_KEEP_START] = (int16_t)(z[Z_SEEN_START] - KEEP_BEHIND);
    }
    const int16_t mask = (int16_t)(race->tri->ring_nodes - 1u);
    static const int order[5] = { Z_AT, Z_SEEN_END, Z_SEEN_START, Z_KEEP_END, Z_KEEP_START };
    for (int32_t k = 0; k < ZONES; k++) {
        int16_t *const z = zone_words(race, k);
        for (int i = 0; i < 5; i++) {
            if (z[order[i]] < 0) z[order[i]] = 0;
        }
        for (int i = 0; i < 5; i++) z[order[i]] &= mask;
    }
}
