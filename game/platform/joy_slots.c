/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: see joy_slots.h. */
#include "game/platform/joy_slots.h"

void joy_slots_match(const uint32_t *old_ids, int old_n, const uint32_t *new_ids, int new_n, int *from_old, int *dropped)
{
    for (int o = 0; o < old_n; o++) dropped[o] = 1;
    for (int n = 0; n < new_n; n++) {
        from_old[n] = -1;
        for (int o = 0; o < old_n; o++) {
            if (old_ids[o] != new_ids[n] || !dropped[o]) continue;   /* the first still-unclaimed old slot with this id */
            from_old[n] = o, dropped[o] = 0;
            break;
        }
    }
}

JoyForces joy_forces_pick(int constant, int sine, int rumble)
{
    if (constant) return JOY_FORCES_HAPTIC;
    if (rumble) return JOY_FORCES_RUMBLE;
    return sine ? JOY_FORCES_HAPTIC : JOY_FORCES_NONE;
}
