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

/* 0043ce88 (eax the player's car; from 00441453 with 0052b320 set): the
 * controls' shake from the speed (+0x68 while the viewed car is a human's,
 * else +0x15c): |speed| x 0x23d70 >> 16 rounded, >> 16 again. 0: both
 * shakes cleared (those on). Else a level and strength from the speed
 * (below 5, 10, 15, 20, 30, 40, 80, 150: 5..12 with 0xf..0x16; else 13,
 * 0x18), by the car's surface (+0x4a2): 1 _XVibration(s - 2, s - 2, l - 2),
 * 2 (s, s, l), 3 (s, s, 1) then _YVibration(0, s - 4, l with l = 1 after
 * an x send), each sent when its shake is off or the pair changed; 0 and
 * any other clear them. The pair kept, except after a send's return. */
void race_send_vibration(const Car *car, RaceState *race);

/* 0043cbe0 (watcall (), from 00455a5c's setup and the pause's end): with
 * the forces on (0052b320, a word), _EnableForces. */
void race_forces_enable(RaceState *race);

/* 0043cbcc (watcall (), from 00455a5c's teardown and the pause): with the
 * forces on, _DisableForces. */
void race_forces_disable(RaceState *race);

/* 0043cbf4 (watcall (), from 00455a5c's setup / teardown and 0043d120):
 * with the forces on, _ClearForces and 004fcdd4 = 0. */
void race_forces_clear(RaceState *race);

#endif
