#include "game/race/police_lights.h"

enum { POLICE_SLOT = 2, ACTIVE = 4, CHASE = 4, SIREN_FLAGS = 0xc, SEARCH_END = 0xf0 };   /* CHASE: flags_175 (ai_police.c); SIREN_FLAGS: flags_176 (car_lights.c) */

int police_lights_on(const RaceState *race, const Car *car)
{
    return race != NULL && car != NULL && race->unknown_4fbab8 != 0 && car->index == POLICE_SLOT && (car->flags_51a & ACTIVE) != 0 &&
           ((car->flags_175 & CHASE) != 0 || (car->flags_176[0] & SIREN_FLAGS) != 0);
}

static uint8_t own_red, own_blue, own_red_halo, own_blue_halo;   /* the entries set aside for the flash (0: none, the palette's nearest are used) */
void police_lights_set_entries(uint8_t red, uint8_t blue, uint8_t red_halo, uint8_t blue_halo)
{
    own_red = red, own_blue = blue, own_red_halo = red_halo, own_blue_halo = blue_halo;
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
    const uint8_t red = own_red != 0 && own_blue != 0 ? own_red : (uint8_t)police_lights_entry(palette, 1),
                  blue = own_red != 0 && own_blue != 0 ? own_blue : (uint8_t)police_lights_entry(palette, 0);
    const int phase = police_lights_phase(tick);
    const uint8_t red_halo = own_red_halo != 0 && own_red != 0 && own_blue != 0 ? own_red_halo : red, blue_halo = own_blue_halo != 0 && own_red != 0 && own_blue != 0 ? own_blue_halo : blue;
    out[POLICE_LIGHT_FIRST] = phase == 0 ? red : blue;          /* the exe's entries 0xfa and 0xfc are its bright ones, 0xfb and 0xfd its darker */
    out[POLICE_LIGHT_FIRST + 1] = phase == 0 ? red_halo : blue_halo;
    out[POLICE_LIGHT_FIRST + 2] = phase == 0 ? blue : red;
    out[POLICE_LIGHT_FIRST + 3] = phase == 0 ? blue_halo : red_halo;
}
