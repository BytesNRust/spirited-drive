/* port (psx_light, the light effects): the police car's lights. The exe writes the four entries 0xfa..0xfd that the car's light texels use (the rear
 * deck's, 270 texels, and the front's, 38) only while a stop notice is up, and the notice's ticket screen covers the 3D then; in the chase the entries
 * hold the dash's colours (0xf0, 13 colours, queued after the lights) and the lights show as small orange, cyan, blue and violet dots that never change.
 * Here, while the police car chases (the siren's chase: police mode on, the car active and chasing), its light texels are drawn pure red and pure blue,
 * the two sides alternating every PSX_POLICE_FLASH_TICKS (the exe's flash cadence), through a table of their own: not the world's light, the night's tint
 * or the fog's. The screen palette is not written (the dash uses the same entries). Off, the exe's. */
#ifndef TNFS_POLICE_LIGHTS_H
#define TNFS_POLICE_LIGHTS_H

#include "game/race/car_struct.h"
#include "game/race/race_state.h"

#include <stdint.h>

enum {
    POLICE_FLASH_TICKS = 15,   /* car_lights.c's FLASH_TICKS */
    POLICE_LIGHT_FIRST = 0xfa, /* the texels' entries 0xfa..0xfd */
    POLICE_LIGHT_COUNT = 4
};

/* 1 when `car` is the police car of a chase that is on: slot 2, police mode (004fbab8), active, and chasing (flags_175 bit 2) or with the siren's flags (flags_176 & 0xc). */
int police_lights_on(const RaceState *race, const Car *car);

/* 0 or 1: the phase of the flash at `tick`; the left lights (0xfa, 0xfb) are red in phase 0, the right ones (0xfc, 0xfd) blue, and the other way round in 1. */
int police_lights_phase(int32_t tick);

/* The entry of `palette` (256 colours, 768 bytes of 8-bit RGB; only entries below 0xf0 are searched, the ones the dash and the lights do not own) nearest
 * to pure red (red = 1) or pure blue (0). */
int police_lights_entry(const uint8_t palette[768], int red);

/* The palette entries set aside for the flash (drive_palette.c), pure red and pure blue past the tint and the tables: police_lights_table uses them when both are
 * set (non-zero), else the palette's nearest entries. (0, 0) forgets them (a race start). */
void police_lights_set_entries(uint8_t red, uint8_t blue);

/* The entry set aside for the flash's red (red = 1) or blue (0), 0 when there is none. */
int police_lights_own_entry(int red);

/* `out` (256 bytes): `base` (NULL for the identity) with the four light entries sent to the red and blue entries of `palette` as the phase of `tick` says. */
void police_lights_table(const uint8_t *base, const uint8_t palette[768], int32_t tick, uint8_t out[256]);

#endif
