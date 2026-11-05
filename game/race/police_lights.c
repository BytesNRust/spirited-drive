#include "game/race/police_lights.h"

#include <stdlib.h>

enum { POLICE_SLOT = 2, ACTIVE = 4, CHASE = 4, SIREN_FLAGS = 0xc, SEARCH_END = 0xf0, STOP_HELD = 1, STOP_NODES = 12 };   /* CHASE: flags_175 (ai_police.c); SIREN_FLAGS: flags_176 (car_lights.c) */

int police_lights_on(const RaceState *race, const Car *car)
{
    if (race == NULL || car == NULL || race->unknown_4fbab8 == 0 || car->index != POLICE_SLOT || (car->flags_51a & ACTIVE) == 0) return 0;
    if ((car->flags_175 & CHASE) != 0 || (car->flags_176[0] & SIREN_FLAGS) != 0) return 1;
    /* port: ai_police.c's stop() clears the chase when the cop has stopped the player, who is held (physics 1); the cop stays lit while the stop lasts */
    const Car *const player = race->cars[0];
    return player != NULL && player != car && player->physics == STOP_HELD && abs(car->node - player->node) <= STOP_NODES;
}

static uint8_t own_red, own_blue, own_red_centre, own_blue_centre;   /* the entries set aside for the flash (0: none, the palette's nearest are used) */
void police_lights_set_entries(uint8_t red, uint8_t blue, uint8_t red_centre, uint8_t blue_centre)
{
    own_red = red, own_blue = blue, own_red_centre = red_centre, own_blue_centre = blue_centre;
}

void police_lights_set_found(const uint8_t entry[4], int found)
{
    if (found < 2) police_lights_set_entries(0, 0, 0, 0);
    else police_lights_set_entries(entry[0], entry[1], found > 2 ? entry[2] : 0, found > 3 ? entry[3] : 0);
}

int police_lights_own_entry(int red) { return red ? own_red : own_blue; }
int police_lights_own_centre(int red) { return red ? own_red_centre : own_blue_centre; }

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
    const int own = own_red != 0 && own_blue != 0;
    const uint8_t red_centre = own && own_red_centre != 0 ? own_red_centre : red, blue_centre = own && own_blue_centre != 0 ? own_blue_centre : blue;
    /* COPMUST.CFM's art: 0xfa and 0xfc are the lamp's centre (the light tints), 0xfb and 0xfd its rim (pure red and blue); the left lamp is 0xfa, 0xfb */
    out[POLICE_LIGHT_FIRST] = phase == 0 ? red_centre : blue_centre;
    out[POLICE_LIGHT_FIRST + 1] = phase == 0 ? red : blue;
    out[POLICE_LIGHT_FIRST + 2] = phase == 0 ? blue_centre : red_centre;
    out[POLICE_LIGHT_FIRST + 3] = phase == 0 ? blue : red;
}

void police_glow_core(int red, uint8_t rgb[3])
{
    static const uint8_t RED[3] = { 63, 54, 50 }, BLUE[3] = { 52, 57, 63 };
    for (int k = 0; k < 3; k++) rgb[k] = red ? RED[k] : BLUE[k];
}

int police_glow_percent(int low, int level)
{
    static const int DAY[3] = { 30, 60, 100 }, DARK[3] = { 45, 90, 140 };   /* the brake lights' halo, and over it where nothing else lights the scene */
    return (low ? DARK : DAY)[level < 0 ? 0 : level > 2 ? 2 : level];
}
