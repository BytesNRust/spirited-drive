/* port (trail_rise): the spray and the dust a car leaves behind it are particles in suspension and drift up: a puff climbs with its age to about a car's height and thins as it climbs.
 * Pure: the age and the life in ticks in, a height and a share out. */
#ifndef TNFS_TRAIL_RISE_H
#define TNFS_TRAIL_RISE_H

#include <stdint.h>

enum { TRAIL_RISE_CAP = 0x16666 };   /* 16.16 metres: 1.4 m, a car's height */

/* How high (16.16 m, 0..TRAIL_RISE_CAP) a puff of `age` ticks of a `life` ticks one has climbed: fast at first, slower as it nears the cap (it eases out). */
int32_t trail_rise_height(int age, int life);

/* How much of its density it keeps (percent, 100 at its birth, falling to 30 by the end of its life), as it rises. */
int trail_rise_fade(int age, int life);

#endif
