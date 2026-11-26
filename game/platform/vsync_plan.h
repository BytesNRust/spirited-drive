/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: when the renderer's vsync can pace the game instead of the software sleep. */
#ifndef TNFS_VSYNC_PLAN_H
#define TNFS_VSYNC_PLAN_H

#include <stdint.h>

/* The swap interval (1..4: present on every Nth vblank) that makes the display's refresh `display_hz` show the game's `target_hz` a second to within 1 %, or 0 when
 * none does (a 144 Hz screen at 60, a 75 Hz one at 60, an unknown refresh: the software pace stays). 59.94 at 60 is 1 (61.8 is not: 3 % fast); 120 at 60 is 2; 60 at 30 is 2. */
int vsync_plan_interval(double display_hz, int target_hz);

/* Whether a present that came back `interval_ns` after the one before it waited for the vblank, for a game frame of `period_ns`: at least half a period. A present that
 * returns at once (the window minimised or hidden) did not, and the software sleep must pace the loop then. */
int vsync_kept_pace(uint64_t interval_ns, uint64_t period_ns);

#endif
