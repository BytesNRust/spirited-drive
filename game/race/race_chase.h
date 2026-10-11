/* The camera that follows a car (004046b0 and the calls under it). */
#ifndef TNFS_RACE_CHASE_H
#define TNFS_RACE_CHASE_H

#include "game/race/car.h"
#include "game/race/race_state.h"
#include "game/race/race_view.h"

#include <stdint.h>

/* The follow record (0x94 bytes; 00511fe0 points at the one in use, e.g.
 * 004f801c). Offsets are the original's. */
typedef struct {
    int32_t kind;               /* +0: the view kind, a 004b6bb0 row (0x58 bytes) */
    int32_t next_kind;          /* +4 */
    int32_t unknown_08;
    int32_t gliding;            /* +0xc: 2 while the eye moves by `step` (00404630) */
    int32_t unknown_10[3];
    int32_t eye[3];             /* +0x1c */
    const int32_t *base;        /* +0x28: the point `offset` is from */
    int32_t offset[3];          /* +0x2c */
    int32_t node;               /* +0x38 */
    int32_t flags;              /* +0x3c */
    int32_t unknown_40[6];
    const int32_t *matrix;      /* +0x58 */
    int32_t turn[3];            /* +0x5c */
    int32_t unknown_68;
    int32_t target[3];          /* +0x6c: where the eye sits from the car */
    int32_t yaw;                /* +0x78 */
    int32_t step[3];            /* +0x7c: added to target each tick while gliding */
    RaceCamera *camera;         /* +0x88 */
    RaceCamera *camera2;        /* +0x8c */
    Car *car;                   /* +0x90: the car followed */
} RaceChase;

/* 00404630: the step that takes target to `to` in `ticks` ticks (divide
 * set) or by a 2^ticks-th of the way; nothing when ticks is 0. */
void race_chase_glide(RaceChase *chase, const int32_t to[3], int32_t divide, int32_t ticks);

/* 00404368: the distance between two points across x and z (the squares
 * rounded, 0042eb54's root with 8 bits, the sum its first guess). */
int32_t race_chase_distance(const int32_t a[3], const int32_t b[3]);

/* 00404544: `from` turned back by the angles (the turns about x, z and y
 * by -pitch, -roll, -yaw, multiplied in that order), at 8 bits less
 * precision: out = ((from >> 8) x M) << 8. */
void race_chase_unturn(const int32_t from[3], int32_t out[3], int32_t pitch, int32_t roll, int32_t yaw);

/* 0043f8f8: 004fbab8 is set, slot 2's car has a flags_174 bit of 0x42400
 * and the followed car (race->player) is more than `after` and at most
 * `upto` nodes ahead of it. */
int race_chase_ahead_of_slot2(const RaceState *race, int32_t after, int32_t upto);

/* 0043f948: slot 1's entry is not car 9 and the followed car is more than
 * `after` and at most `upto` nodes ahead of the other of slots 0 and 1. */
int race_chase_ahead_of_other(const RaceState *race, int32_t after, int32_t upto);

#endif
