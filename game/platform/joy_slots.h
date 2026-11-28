/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: the pads' slots when the devices change (a pad plugged in or out while the game runs): the game names a pad by its place in the list, so the ones still there keep what is open for them and the ones gone are closed. */
#ifndef TNFS_JOY_SLOTS_H
#define TNFS_JOY_SLOTS_H

#include <stdint.h>

/* `old_ids` (`old_n`) and `new_ids` (`new_n`) are the lists of device ids before and after. For each new slot n, from_old[n] is the old slot with the same id, or -1 (a new device). For each old slot o, dropped[o] is 1
 * when its id is not in the new list. The arrays are as long as their lists. */
void joy_slots_match(const uint32_t *old_ids, int old_n, const uint32_t *new_ids, int new_n, int *from_old, int *dropped);

/* The way a device's forces are felt. A haptic device with a constant force is a force-feedback wheel or stick: its own effects. Otherwise rumble when the device has it: Linux
 * calls an ordinary pad haptic too, with a sine the kernel makes out of rumble and no constant force, so the knocks and pushes would be lost there. Then a haptic device
 * with a sine alone. */
typedef enum { JOY_FORCES_NONE, JOY_FORCES_HAPTIC, JOY_FORCES_RUMBLE } JoyForces;

JoyForces joy_forces_pick(int constant, int sine, int rumble);

#endif
