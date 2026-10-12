/* The followed car's wheel frames (00458614, run by the car draw 00458d4c
 * for cars[004fbb08] only): tyr1..4 by the speed, still while sliding,
 * blurred while spinning, bound to the 'frntires' and 'rertires' refs. */
#ifndef TNFS_CAR_WHEELS_H
#define TNFS_CAR_WHEELS_H

#include "game/race/car.h"
#include "game/race/race_state.h"

/* 00490df3 as the caller has it: ref `slot` of `object` (car +0x52e) now
 * draws `frame` (one of car->wheel_frames). */
typedef void (*CarBindTexture)(void *user, uint32_t object, int32_t slot, uint32_t frame);

/* 00458614: frames 0 (still or sliding), 1 and 2 (turning, alternating each
 * tick below 0xa0000, by the tick parity above), 3 (fast, or driven past
 * grip); the front wheels of models other than 1 and 10 count as still
 * when the car moved under 0x1999 since the last call. A frame that
 * changed is bound. */
void car_wheel_frames(Car *car, RaceState *race, CarBindTexture bind, void *user);

/* 00458818's part move as the caller has it: 004914b8(object, part,
 * weight) on the car's body object (+0x52e). */
typedef void (*CarMorphPart)(void *user, uint32_t object, int32_t part, int32_t weight);

/* 004587ac: the angle the follow camera (race->chase.position) sees the car
 * from, against its heading (+0x351), 24-bit, -0x800000..0x800000. */
int32_t car_view_angle(const Car *car, const RaceState *race);

/* 00458818 (the car draw 00458d4c, for cars[004fbb08]): the front wheels
 * moved by the steering (+0x18 / 32, toward full lock as a weight), the
 * left one (+0x60e) while the camera sees the car from 0xa0000..0x760000,
 * the right one (+0x612) from -0x760000..-0xa0000; the other at rest. */
void car_steer_wheels(const Car *car, const RaceState *race, CarMorphPart morph, void *user);

#endif
