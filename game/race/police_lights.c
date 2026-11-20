#include "game/race/police_lights.h"

#include <stdlib.h>

enum { POLICE_SLOT = 2, ACTIVE = 4, CHASE = 4, SIREN_FLAGS = 0xc, STOP_HELD = 1, STOP_NODES = 12 };   /* CHASE: flags_175 (ai_police.c); SIREN_FLAGS: flags_176 (car_lights.c) */

int police_lights_on(const RaceState *race, const Car *car)
{
    if (race == NULL || car == NULL || race->unknown_4fbab8 == 0 || car->index != POLICE_SLOT || (car->flags_51a & ACTIVE) == 0) return 0;
    if ((car->flags_175 & CHASE) != 0 || (car->flags_176[0] & SIREN_FLAGS) != 0) return 1;
    /* port: ai_police.c's stop() clears the chase when the cop has stopped the player, who is held (physics 1); the cop stays lit while the stop lasts */
    const Car *const player = race->cars[0];
    return player != NULL && player != car && player->physics == STOP_HELD && abs(car->node - player->node) <= STOP_NODES;
}
