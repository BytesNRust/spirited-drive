#include "game/race/crash_trail.h"

#include "game/sys/fixed_math.h"

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
        trail->drift[i] = ((int32_t)(r >> 15) >> 8) << 7;
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

int32_t crash_trail_age(CrashTrail *trail, int32_t dt)
{
    const int32_t step = dt >> 8, swirl = dt >> 9;
    int32_t alive = 0;
    for (int i = 0; i < CRASH_TRAIL_POINTS; i++) {
        if (trail->age[i] < 0) continue;
        const int32_t age = trail->age[i] + step * 15;
        trail->age[i] = age;
        if (age >= 0x10000) {
            trail->age[i] = -0x10000;
            continue;
        }
        alive = 1;
        if (age < 0x4000) {
            const int32_t rise = age * 4 >> 8;
            trail->position[i][1] += ((rise * rise >> 8) * step >> 8) * 0x180;
        } else {
            trail->position[i][1] += step * 0x180;
            trail->position[i][0] += (trail->drift[i] >> 8) * step;
            trail->position[i][0] += (fx_sin16((age << 9) >> 8) >> 8) * swirl;
            trail->position[i][2] += (fx_cos16((age << 9) >> 8) >> 8) * swirl;
        }
    }
    return alive;
}
