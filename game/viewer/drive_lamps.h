/* port (emissive lamps, lamp_emissive.h): the race's cars' lamp texels, their palette entries and their state. A car's lamp texels are found when its
 * file is loaded (drive_lamps_find), the day indices read once the palette has mapped the cars (drive_lamps_arm), and the texels pointed at the glowing
 * entries or back at the day's as the light at the car changes (drive_lamps_frame). Nothing is found, armed or written with the port's key off, or
 * for a car with no mask that marks a lamp. */
#ifndef TNFS_DRIVE_LAMPS_H
#define TNFS_DRIVE_LAMPS_H

#include "game/gfx/object3d.h"
#include "game/race/lamp_emissive.h"

#include <stddef.h>
#include <stdint.h>

enum { DRIVE_LAMP_SLOTS = 10 };

/* Forgets the slot's texels (a car is loaded into it). */
void drive_lamps_clear(int slot);
/* Forgets every slot's texels and the pointer to the file they were found in: the race is over and its car files are freed, so a slot no car is loaded into next
 * (an empty one, a traffic car fewer) must not be armed over a freed file. */
void drive_lamps_clear_all(void);
/* The slot's texels from the masks of `stem` over `file`. Returns how many. */
int drive_lamps_find(int slot, const char *stem, uint8_t *file, size_t size);
/* Whether the slot has texels of a kind (LampKind), found. */
int drive_lamps_has(int slot, int kind);
/* Reads the day indices; the cars' palette must be mapped. */
void drive_lamps_arm(void);
/* How many of the slot's lamp texels lie in a texture the object draws (its bound textures' payloads inside the car's file): fewer than the slot's all
 * means the object draws a copy, or other art, that the toggle does not reach. */
int drive_lamps_reached(int slot, const Object3D *obj);
int drive_lamps_count(int slot);

/* The shared glowing entries (0: none): the lit brake light, the position light, the headlight and the reversing light. */
void drive_lamps_set_entries(uint8_t brake_lit, uint8_t position, uint8_t headlight, uint8_t reverse);
/* Points the slot's texels at the glowing entries or the day's, by the light at its car, its brake and its gear (`reversing`: the reversing light is lit, in any
 * light). Cheap when nothing changed. */
void drive_lamps_frame(int slot, int low, int braking, int reversing);
/* The palette entry the slot's brake texels show while they are lit (low light, the brake down), else 0: what the lamp's halo (lamp_flare.h) is found by. */
int drive_lamps_brake_lit(int slot);
/* The same from the light itself: the world's tint and the ambient ratio at the car (NULL for none), through lamp_light_step's margin (a light shade by day does
 * not light them; the slot's state starts out at drive_lamps_arm; each flip is logged with the node). */
void drive_lamps_light(int slot, int node, const float tint[3], const float ratio[3], int braking, int reversing);
#endif
