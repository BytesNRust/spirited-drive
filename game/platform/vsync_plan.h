/* SPDX-License-Identifier: GPL-3.0-or-later */
/* port: when the renderer's vsync can pace the game instead of the software sleep. */
#ifndef TNFS_VSYNC_PLAN_H
#define TNFS_VSYNC_PLAN_H

/* The swap interval (1..4: present on every Nth vblank) that makes the display's refresh `display_hz` show the game's `target_hz` a second to within 3 %, or 0 when
 * none does (a 144 Hz screen at 60, a 75 Hz one at 60, an unknown refresh: the software pace stays). 59.94 at 60 is 1; 120 at 60 is 2; 60 at 30 is 2. */
int vsync_plan_interval(double display_hz, int target_hz);

#endif
