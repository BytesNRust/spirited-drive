/* The pad as the race reads it (0043d598): the stick's x and y about their
 * calibrated middles, scaled to the steering's and the pedals' ranges. */
#ifndef TNFS_JOY_PAD_H
#define TNFS_JOY_PAD_H

#include "game/sys/joy_start.h"
#include "game/sys/joystick.h"

/* 0043d598 (eax player, edx stand-in, neither read; all registers kept):
 * without the pad present (004fcdcc) or nothing polled (0043d8d0): 004fce68,
 * 004fce54 and 004fce58 0. Else 004fce68 the buttons; the four axes turned
 * over (0043db50) unless 004fce18; x and y less their mids, z too without an
 * r axis (004fcdc4). x: below 0 times its low scale (16.16, the product's
 * 32 bits), +3, kept to -0x20..0; else times its high scale, -3, kept to
 * 0..0x1f. y the same within -0x1e..0x1e. With an r axis the pedals are z
 * and r: each 0x24 less (its half times its pedal scale), less 5, at least
 * 0; y = r less z within -0x1e..0x1e. Else with a z axis (004fcdc0) y is z
 * scaled as y but -4 above. Then 004fce54 x, 004fce58 y. */
void joy_pad_read(JoyState *j, const JoyStart *s, const JoyOps *joy);

#endif
