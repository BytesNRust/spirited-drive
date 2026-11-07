#include "game/race/race_state.h"
#include "game/race/weather.h"

#include "game/race/track_gravity.h"

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

static Surface surface(int32_t grip, int32_t rolling, int32_t drag, int32_t off_road)
{
    return (Surface){ grip, rolling, drag, off_road };
}

/* The second and third surfaces' off_road is left alone on track 2 variant
 * 2, as the original does. */
void race_surfaces_init(RaceState *race)
{
    if (race->track == 2 && race->track_variant == 2) {
        race->surfaces[0] = surface(0x100, 0x100, 0x3333, 0);
        race->surfaces[1] = surface(0x4c, 0x100, 0x1999, 1);
        race->surfaces[2].grip = 0x4c, race->surfaces[2].rolling = 0x100, race->surfaces[2].drag = 0x1999;
    } else if (race->track >= 4) {
        race->surfaces[0] = surface(0x100, 0x100, 0x3333, 0);
        race->surfaces[1] = surface(0x133, 0x100, 0x3333, 1);
        race->surfaces[2] = surface(0x133, 0x100, 0x3333, 1);
    } else if (race->track == 3) {
        race->surfaces[0] = surface(0x100, 0x100, 0x3333, 0);
        race->surfaces[1] = surface(0x100, 0x1400, 0x3333, 1);
        race->surfaces[2] = surface(0x100, 0x1400, 0x3333, 1);
    } else {
        race->surfaces[0] = surface(0x100, 0x100, 0x3333, 0);
        race->surfaces[1] = surface(0xcc, 0xf00, 0x5999, 1);
        race->surfaces[2] = surface(0xcc, 0xf00, 0x5999, 1);
    }
    if (race->arcade)
        for (int i = 0; i < RACE_SURFACES; i++) race->surfaces[i] = surface(0x180, 0x80, 0x1999, 1);
    if (race->port_lunar_gravity)   /* port: the moon's grip */
        for (int i = 0; i < RACE_SURFACES; i++) race->surfaces[i].grip = track_grip(race->port_lunar_gravity, race->surfaces[i].grip);
    for (int i = 0; i < RACE_SURFACES; i++) race->port_grip_dry[i] = race->surfaces[i].grip;
    if (race->port_rain_grip)   /* port (rain_grip): the wet road, not the shoulder */
        for (int i = 0; i < RACE_SURFACES; i++)
            if (race->surfaces[i].off_road == 0 || race->arcade) race->surfaces[i].grip = track_rain_grip(race->port_rain_grip, race->surfaces[i].grip);
}

int32_t race_car_grip(const RaceState *race, const Car *car)
{
    if (car == NULL || car->surface < 0 || car->surface >= RACE_SURFACES) return 0;
    const Surface *s = &race->surfaces[car->surface];
    if (!race->port_rain_grip || race->port_open == NULL || race->tri == NULL || race->tri->ring_nodes == 0) return s->grip;
    return weather_wet_grip(race->port_grip_dry[car->surface], s->grip, race->port_open[(uint32_t)car->node % race->tri->ring_nodes]);
}

void race_dash_gauges(RaceState *race, int32_t a, int32_t b)
{
    if (a > 500) a = 0;
    if (b > 500) b = 0;
    if (a > 0) race->gauge_acc = a * 0xa76 / 0x2ee0 * 33 / 10;
    if (b > 0) race->gauge_ac8 = b * 0xdf3 / 0x2ee0 * 33 / 10;
}

/* 0041e7fc */
void race_dash_values(RaceState *race, int32_t a, int32_t b, int32_t c, int32_t d)
{
    if (a > 0x4b0) a = 0;
    if (b > 0x4b0) b = 0;
    if (c > 0x4b0) c = 0;
    if (a > 0) race->dash_ab8 = a;
    if (b > 0) race->dash_abc = b;
    if (c > 0) race->dash_ac0 = c;
    if (d > 0) race->dash_ac4 = d * 224 / 0x640000;
}

void race_blow_up(RaceState *race)
{
    if (race->game_mode != 4 || race->tick <= 300 || race->sounds.push == NULL) return;
    for (int32_t car = 1; car < race->car_count; car++)
        race->sounds.push(race->sounds.user, car, 0xa0000, 0xa0000, 0xa0000);
}

int32_t race_gear_step(const RaceState *race, int32_t gear)
{
    static const int32_t STEPS[8] = { 67, 67, 67, 119, 164, 212, 263, 341 };
    if (gear >= -2 && gear <= 5) return STEPS[gear + 2];
    return gear == 6 ? race->wheelspin : 0;
}

void race_random_seed(RaceState *race, uint32_t seed)
{
    race->random_seed = seed * 3 | 1;
    race->random_mul = seed | 1;
}

uint32_t race_random(RaceState *race)
{
    race->random_last = race->random_seed * race->random_mul;
    race->random_seed = race->random_last & 0xffff;
    return (race->random_last & 0xffff00) >> 8;
}

uint32_t race_rand(RaceState *race)
{
    uint32_t s[6];
    for (int i = 0; i < 6; i++) s[i] = (uint32_t)race->rand_state[i];
    uint64_t sum = (uint64_t)s[5] + s[4];
    s[4] = (uint32_t)sum;
    for (int i = 3; i >= 1; i--) {
        sum = (uint64_t)s[i + 1] + s[i] + (sum >> 32);
        s[i] = (uint32_t)sum;
    }
    uint32_t top = (uint32_t)((uint64_t)s[1] + s[0] + (sum >> 32));
    if (++s[5] == 0 && ++s[4] == 0 && ++s[3] == 0 && ++s[2] == 0 && ++s[1] == 0) top++;
    s[0] = top;
    for (int i = 0; i < 6; i++) race->rand_state[i] = (int32_t)s[i];
    return top;
}

int32_t race_rand_below(RaceState *race, int32_t n)
{
    return (int32_t)((race_rand(race) & 0xffff) * (uint32_t)n) >> 16;
}

int32_t race_loading_pick(RaceState *race, int clear)
{
    enum { PICTURES = 7 };
    if (clear) memset(race->loading_used, 0, sizeof race->loading_used);
    int left = 0;
    for (int i = 0; i < PICTURES; i++) left |= !race->loading_used[i];
    if (!left) return -1;  /* port guard */
    int32_t n;
    do n = race_rand_below(race, PICTURES);
    while (race->loading_used[n]);
    race->loading_used[n] = 1;
    return n;
}

Car *race_car_by_handle(const RaceState *race, uint32_t handle)
{
    for (int i = 0; i < RACE_CARS; i++)
        if (race->cars[i] != NULL && race->cars[i]->unknown_088 == handle) return race->cars[i];
    return NULL;
}

/* Bubble sort of slots by a car field, the original's way (ties by +0x39d). */
static void sort_slots(RaceState *race, int32_t slots[RACE_CARS], int by_progress)
{
    const int last = (race->car_count > RACE_CARS ? RACE_CARS : race->car_count) - 1;  /* port guard */
    int swapped;
    do {
        swapped = 0;
        for (int i = 0; i < last; i++) {
            const Car *a = race->cars[race_slot(slots[i])];
            const Car *b = race->cars[race_slot(slots[i + 1])];
            const int32_t ka = by_progress ? a->progress : a->node, kb = by_progress ? b->progress : b->node;
            if (kb < ka || (ka == kb && b->lateral < a->lateral)) {
                const int32_t t = slots[i];
                slots[i] = slots[i + 1];
                slots[i + 1] = t;
                swapped = 1;
            }
        }
    } while (swapped);
}

void race_sort_cars(RaceState *race)
{
    sort_slots(race, race->by_node, 0);
    sort_slots(race, race->by_progress, 1);
}
