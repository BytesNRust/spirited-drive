/* The race camera's choice of view (0045573c, 00405b00, 00405de8). */
#ifndef TNFS_RACE_CAMERA_H
#define TNFS_RACE_CAMERA_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 00405de8: the car points along its node's heading within 0x1e0000 and
 * its pitch and roll are both within 0x240000 (a 24-bit turn). */
int car_faces_road(const Car *car, const RaceState *race);

/* 0045573c: each tick, the view the followed `car` (cars[viewed]) calls
 * for. camera_hold counts down. Unless chase sits in its chosen kind
 * (next_kind) above 3 and not 10: a car not facing the road (00405de8)
 * holds the camera 60 ticks and takes 13 when in kinds 0..3, 10 or 11;
 * else, out of its chosen kind and with no hold, it takes that kind back
 * (0 as 10, 1 as 11) through 00405b00. */
void race_camera_pick(RaceState *race, const Car *car);

/* 0043ce08: the followed car's lateral and along forces (+0x411, +0x415)
 * >> 13 as the controls' force: x kept in -100..100, y outside it becomes
 * +100 either way; sent only when it differs from the last. */
void race_send_force(const Car *car, RaceState *race);

#endif
