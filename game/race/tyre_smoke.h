/* The smoke off a skidding car's tyres (00458988, from the car draw
 * 00458d4c): puffs at its newest skid marks (004588ac). */
#ifndef TNFS_TYRE_SMOKE_H
#define TNFS_TYRE_SMOKE_H

#include "game/race/car_struct.h"

#include <stdint.h>

typedef struct {
    void *ctx;
    int32_t (*view_angle)(void *ctx, const Car *car);           /* 004587ac (car_view_angle) */
    int32_t (*rand_below)(void *ctx, int32_t n);                /* 00443730 */
    void (*puffs)(void *ctx, int32_t corner, int32_t size);     /* 004588ac (skid_puffs) */
} TyreSmokeOps;

/* 00458988 (watcall (car, in_car)): nothing unless a tyre flag (+0x425 /
 * +0x426) and a skid (+0x405 / +0x409) are set, nor for a far car (+0x78e);
 * the angle the camera sees it from is taken first. base = (in_car and
 * roll_add (004c53a8) 0 ? 0x80 : 0x100) x (0x38400 / camera z, +0x399); for
 * each pair with a skid, front then rear: n = skid x (rand(4) + 2) / 2^18,
 * 1..0x180, size = base x n; its left puffs (corner 0 / 2) when in_car or
 * the angle is in 0xc0000..0x740000, its right (1 / 3) in
 * -0x740000..-0xc0000 (ends out). */
void tyre_smoke(const TyreSmokeOps *ops, const Car *car, int in_car, int32_t roll_add);

#endif
