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

/* 0 or 1: the phase of the flash at `tick`; the left lamp (0xfa, 0xfb) is red in phase 0, the right one (0xfc, 0xfd) blue, and the other way round in 1. */
int police_lights_phase(int32_t tick);

/* The entry of `palette` (256 colours, 768 bytes of 8-bit RGB; only entries below 0xf0 are searched, the ones the dash and the lights do not own) nearest
 * to pure red (red = 1) or pure blue (0). */
int police_lights_entry(const uint8_t palette[768], int red);

/* The palette entries set aside for the flash (drive_palette.c), past the tint and the tables. The police car's art (COPMUST.CFM, its !PAL and the light texels of
 * its 'rear' and 'back' shapes) draws each lamp as a centre of a light tint (0xfa red (252,100,84), 0xfc blue (76,124,252)) inside a rim of the pure colour (0xfb red
 * (252,0,0), 0xfd blue (0,0,252)); the rim has the most texels (rear: 94 and 102 against 37 and 37). `red` and `blue` are the rims' entries, `red_centre` and
 * `blue_centre` the centres': police_lights_table uses them when both rims are set (non-zero), else the palette's nearest entries; a centre not set is its rim.
 * (0, 0, 0, 0) forgets them (a race start). */
void police_lights_set_entries(uint8_t red, uint8_t blue, uint8_t red_centre, uint8_t blue_centre);

/* The same from the `found` entries (2 to 4) the palette had free, in the order rim red, rim blue, centre red, centre blue: a centre not found is 0 (its rim). Fewer than 2 forgets them all. */
void police_lights_set_found(const uint8_t entry[4], int found);

/* The entry set aside for the flash's red (red = 1) or blue (0), 0 when there is none. */
int police_lights_own_entry(int red);

/* `out` (256 bytes): `base` (NULL for the identity) with the four light entries sent to the red and blue entries of `palette` as the phase of `tick` says. */
void police_lights_table(const uint8_t *base, const uint8_t palette[768], int32_t tick, uint8_t out[256]);

#endif
