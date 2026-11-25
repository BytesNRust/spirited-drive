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
