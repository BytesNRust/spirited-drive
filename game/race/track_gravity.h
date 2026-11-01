/* port: the moon's gravity and grip (lunar_springs.h, extras.cfg's lunar_gravity). In the moon's race (RaceState's
 * port_lunar_gravity) a car is pulled down a third as hard as the exe's pull (the unit and its half, car_vertical.c) and
 * its surfaces grip at two thirds of their own. Both are identity with the flag at 0, so the exe's numbers stay byte for
 * byte; the exe's own TRACK_FLAG_LOW_GRAVITY (a track's smaller pull) is left as it is. */
#ifndef TNFS_TRACK_GRAVITY_H
#define TNFS_TRACK_GRAVITY_H

#include <stdint.h>

enum {
    LOW_GRAVITY_DIV = 3,       /* the moon's pull: 1/3 of the exe's */
    LOW_GRIP_NUM = 2,          /* the moon's grip: 2/3 of the surface's own (0x180 on the rally: 0x100) */
    LOW_GRIP_DEN = 3
};

/* `pull` as the race pulls it: the same with `moon` 0, a third of it with 1 */
int32_t track_gravity(int32_t moon, int32_t pull);

/* a surface's grip (8.8) as the race grips it */
int32_t track_grip(int32_t moon, int32_t grip);

#endif
