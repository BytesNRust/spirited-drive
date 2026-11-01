/* port (port_follow_camera, extras "Follow camera", experimental): the chase and in-car cameras follow the car's own heading,
 * through a spring, at every angle to the road. The exe turns its chase camera to a road-fixed one when the car is more than
 * 42 degrees off the road (00405de8) and so never turns round; this takes that out of 004046b0's follow_road (race_chase.c). */
#ifndef TNFS_RACE_FOLLOW_H
#define TNFS_RACE_FOLLOW_H

#include "game/race/chase_struct.h"

#include <stdint.h>

/* One tick of the camera's yaw spring toward the heading of `car` (the chase's followed car), its own state in `chase`
 * (started at the car's heading for a car it was not following); the yaw, a 24-bit angle. */
int32_t race_follow_yaw(RaceChase *chase, const Car *car);

/* Whether the look back is held: only with the 360 camera (`follow_camera`), by the keyboard's key (`key`, a scancode, 0 none) or the pad's
 * (RACE_KEYS_PAD_LOOK_BACK); `keys` is a flag per key code, `codes` how many it has. */
int32_t race_look_back_held(int32_t follow_camera, const uint8_t *keys, int32_t codes, int32_t key);

/* The direction (x, 0, z) of a camera yaw, as 004046b0's road direction is for the exe's: angle_of(dir[2], dir[0]) is `yaw`. */
void race_follow_direction(int32_t yaw, int32_t dir[3]);

/* The eye `after` the road's barriers moved it from `before` (both x, z; `car` the followed car's): when the move brought it closer to the
 * car than it was, it is slid along the barrier (the line through `after` square to the move) to where it is as far from the car as
 * `before`, so the camera swinging round the car keeps its distance instead of cutting across it. Left as it is when the barrier does
 * not move it or the car is outside the barrier. */
void race_follow_keep_radius(const int32_t car[2], const int32_t before[2], int32_t after[2]);

#endif
