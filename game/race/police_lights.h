/* port (light_effects): when the police car's lights are on (the flash itself, its phase, table and glow, is formats/police_flash.h, included here). */
#ifndef TNFS_POLICE_LIGHTS_H
#define TNFS_POLICE_LIGHTS_H

#include "formats/police_flash.h"
#include "game/race/car_struct.h"
#include "game/race/race_state.h"

/* 1 when `car` is the police car of a chase that is on: slot 2, police mode (004fbab8), active, and chasing (flags_175 bit 2) or with the siren's flags (flags_176 & 0xc). */
int police_lights_on(const RaceState *race, const Car *car);

#endif
