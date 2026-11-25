/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: the pads' slots when the devices change (a pad plugged in or out while the game runs): the game names a pad by its place in the list, so the ones still there keep what is open for them and the ones gone are closed. */
#ifndef TNFS_JOY_SLOTS_H
#define TNFS_JOY_SLOTS_H

#include <stdint.h>

/* `old_ids` (`old_n`) and `new_ids` (`new_n`) are the lists of device ids before and after. For each new slot n, from_old[n] is the old slot with the same id, or -1 (a new device). For each old slot o, dropped[o] is 1
 * when its id is not in the new list. The arrays are as long as their lists. */
void joy_slots_match(const uint32_t *old_ids, int old_n, const uint32_t *new_ids, int new_n, int *from_old, int *dropped);

#endif
