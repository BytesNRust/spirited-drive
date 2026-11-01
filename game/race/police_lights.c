#include "game/race/police_lights.h"

enum { POLICE_SLOT = 2, ACTIVE = 4, CHASE = 4, SIREN_FLAGS = 0xc, SEARCH_END = 0xf0 };   /* CHASE: flags_175 (ai_police.c); SIREN_FLAGS: flags_176 (car_lights.c) */

int police_lights_on(const RaceState *race, const Car *car)
{
    return race != NULL && car != NULL && race->unknown_4fbab8 != 0 && car->index == POLICE_SLOT && (car->flags_51a & ACTIVE) != 0 &&
           ((car->flags_175 & CHASE) != 0 || (car->flags_176[0] & SIREN_FLAGS) != 0);
}

int police_lights_phase(int32_t tick) { return (int)(((uint32_t)tick / POLICE_FLASH_TICKS) & 1u); }

int police_lights_entry(const uint8_t palette[768], int red)
{
    int best = 0;
    long best_distance = -1;
    for (int e = 0; e < SEARCH_END; e++) {
        const int r = palette[3 * e], g = palette[3 * e + 1], b = palette[3 * e + 2];
        const int dr = red ? 255 - r : r, dg = g, db = red ? b : 255 - b;
        const long distance = (long)dr * dr + (long)dg * dg + (long)db * db;
        if (best_distance < 0 || distance < best_distance) best = e, best_distance = distance;
    }
    return best;
}

void police_lights_table(const uint8_t *base, const uint8_t palette[768], int32_t tick, uint8_t out[256])
{
    for (int i = 0; i < 256; i++) out[i] = base != NULL ? base[i] : (uint8_t)i;
    const uint8_t red = (uint8_t)police_lights_entry(palette, 1), blue = (uint8_t)police_lights_entry(palette, 0);
    const int phase = police_lights_phase(tick);
    out[POLICE_LIGHT_FIRST] = out[POLICE_LIGHT_FIRST + 1] = phase == 0 ? red : blue;
    out[POLICE_LIGHT_FIRST + 2] = out[POLICE_LIGHT_FIRST + 3] = phase == 0 ? blue : red;
}
