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

#endif
