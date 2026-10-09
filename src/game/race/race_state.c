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
}

void race_dash_gauges(RaceState *race, int32_t a, int32_t b)
{
    if (a > 500) a = 0;
    if (b > 500) b = 0;
    if (a > 0) race->gauge_acc = a * 0xa76 / 0x2ee0 * 33 / 10;
    if (b > 0) race->gauge_ac8 = b * 0xdf3 / 0x2ee0 * 33 / 10;
}

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

uint32_t race_random(RaceState *race)
{
    race->random_last = race->random_seed * race->random_mul;
    race->random_seed = race->random_last & 0xffff;
    return (race->random_last & 0xffff00) >> 8;
}

Car *race_car_by_handle(const RaceState *race, uint32_t handle)
{
    for (int i = 0; i < RACE_PLAYERS; i++)
        if (race->cars[i] != NULL && race->cars[i]->unknown_088 == handle) return race->cars[i];
    return NULL;
}
