/* The points a crashing car leaves behind (the block at 005146ac): a ring of
 * 100, dropped along the body's path at random gaps (00456cc4), aged and
 * drawn by 00456e38 / 00456f34, cleared by 00456a48. */
#ifndef TNFS_CRASH_TRAIL_H
#define TNFS_CRASH_TRAIL_H

#include "game/race/car.h"

#include <stdint.h>

enum { CRASH_TRAIL_POINTS = 100 };

#pragma pack(push, 1)
typedef struct CrashTrail {
    int32_t position[CRASH_TRAIL_POINTS][3];  /* 0x000 the body's place (z negated) give or take 0x8000 */
    int32_t age[CRASH_TRAIL_POINTS];          /* 0x4b0 0 when dropped, -1 when cleared */
    int32_t unknown_640[CRASH_TRAIL_POINTS];  /* 0x640 */
    int32_t spread[CRASH_TRAIL_POINTS];       /* 0x7d0 random, 0..0xff80 in steps of 0x80 */
    int32_t next;                             /* 0x960 the ring slot dropped next */
    int32_t travelled;                        /* 0x964 distance since the last drop */
    int32_t gap;                              /* 0x968 distance to the next drop */
} CrashTrail;
#pragma pack(pop)
typedef char crash_trail_size_check[sizeof(CrashTrail) == 0x96c ? 1 : -1];

/* 00456cc4: the car moved `step` further; drop a point each time the
 * distance passes the gap. `seed` is the effect's own generator (004c5608). */
void crash_trail_drop(CrashTrail *trail, uint32_t *seed, const Car *car, int32_t step);

#endif
