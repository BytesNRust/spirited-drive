#include "game/race/crash_trail.h"

/* 004c5608's generator */
static uint32_t next_random(uint32_t seed)
{
    return seed * 0x10004005u + 0x11;
}

/* A random offset of -0x8000..0x7f80 in steps of 0x80. */
static int32_t jitter(uint32_t r)
{
    return ((int32_t)(0x10000 - (r >> 15)) >> 8) * 0x80;
}

void crash_trail_drop(CrashTrail *trail, uint32_t *seed, const Car *car, int32_t step)
{
    trail->travelled += step;
    while (trail->gap < trail->travelled) {
        const int32_t i = trail->next;
        uint32_t r = next_random(*seed);
        trail->spread[i] = ((int32_t)(r >> 15) >> 8) << 7;
        trail->position[i][0] = car->body_position[0];
        trail->position[i][1] = car->body_position[1];
        trail->position[i][2] = -car->body_position[2];
        for (int k = 0; k < 3; k++) {
            r = next_random(r);
            trail->position[i][k] += jitter(r);
        }
        trail->age[i] = 0;
        trail->next = i + 1 == CRASH_TRAIL_POINTS ? 0 : i + 1;
        *seed = next_random(r);
        trail->travelled -= trail->gap;
        trail->gap = (int32_t)((*seed >> 23) * 0x6666) >> 8;
    }
}
