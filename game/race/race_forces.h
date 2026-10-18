/* The force-feedback stick's effects as the race asks for them (0043cc84,
 * 0043d120): which jolt a hit gives, and the buffeting on and off. The
 * stick itself is the platform's (IFORCE.DLL in the exe), behind RaceSounds'
 * ops. */
#ifndef TNFS_RACE_FORCES_H
#define TNFS_RACE_FORCES_H

#include "game/race/race_state.h"

typedef struct RaceForces {
    int32_t jolt_at;  /* 004bb8f4: the clock (004bb8e0) of the last jolt */
} RaceForces;

/* 0043cc84 (watcall eax car a, edx car b, then x, y, z, the impulse on the
 * stack; ret 0x10): a car-to-car hit felt by the followed car. None within
 * 0x19 ticks of the last (unless the clock went back, which starts again
 * from it); else _Jolt (sounds' tone) with the strength and the time by the
 * impulse (below 0x2000 0x50 / 100, 0x5000 0x55 / 150, 0x10000 0x5a / 200,
 * 0x100000 0x5f / 240, else 0x64 / 280), from 0x2d when x is above 0 and car
 * a's player (+0x47d) is not the followed one (004fbb04), else from 0x13b. */
void race_jolt(RaceForces *f, int32_t clock, int32_t viewed, int32_t player_a, int32_t x, int32_t impulse,
               const RaceSounds *sounds);

#endif
