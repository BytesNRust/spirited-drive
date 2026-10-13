#include "game/race/crash_trail.h"

#include "game/gfx/x86_int.h"
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

void crash_trail_clear(CrashTrail *trail)
{
    for (int i = 0; i < CRASH_TRAIL_POINTS; i++) trail->age[i] = -1;
    trail->gap = 0;
    trail->travelled = 0;
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

enum { Z_LEAST = 1, Z_MOST = 0x7fff, NEAREST = 0x4ccc, LIFE = 0x10000, FADE_FLOOR = 0xccc, PUFF_SIZE = 0x300 };

/* (a << 16) / z as 16.16, the quotient and then the remainder's fraction (two idivs) */
static int32_t ratio(int32_t a, int32_t z)
{
    const int32_t q = a / z, r = a % z;
    return x_add(x_shl(q, 16), (int32_t)(((int64_t)r * 65536) / z));
}

void crash_trail_draw(CrashTrail *trail, const int32_t offset[2], const CrashTrailView *v)
{
    const int shift = v->world_shift & 31;
    const int32_t eye[3] = { x_sub(0, v->position[0]) >> shift, x_sub(0, v->position[1]) >> shift,
                             x_sub(0, v->position[2]) >> shift };
    int32_t cam[CRASH_TRAIL_POINTS][3];
    for (int i = 0; i < CRASH_TRAIL_POINTS; i++) {
        const int32_t t[3] = { x_add(trail->position[i][0] >> shift, eye[0]), x_add(trail->position[i][1] >> shift, eye[1]),
                               x_add(trail->position[i][2] >> shift, eye[2]) };
        mat3_apply(t, v->matrix, cam[i]);
        if (cam[i][2] <= 0) cam[i][2] = Z_LEAST;
        else if (cam[i][2] > Z_MOST) cam[i][2] = Z_MOST;
    }
    int32_t *order = trail->unknown_640;
    for (int k = 1; k < CRASH_TRAIL_POINTS; k++)
        for (int j = k; j > 0 && cam[order[j]][2] > cam[order[j - 1]][2]; j--) {
            const int32_t o = order[j];
            order[j] = order[j - 1], order[j - 1] = o;
        }
    int32_t screen[CRASH_TRAIL_POINTS][2];
    for (int i = 0; i < CRASH_TRAIL_POINTS; i++) view_project(v->view, cam[i], screen[i]);
    const int32_t focal = x_shl(v->view->focal_x, (16 - shift) & 31);
    for (int k = 0; k < CRASH_TRAIL_POINTS; k++) {
        const int32_t i = order[k];
        if (trail->age[i] < 0 || cam[i][2] <= NEAREST >> shift) continue;
        const int32_t scale = ratio(focal, cam[i][2]);
        const int32_t left = x_sub(LIFE, trail->age[i]) >> 8;
        const int32_t fade = x_add(x_sub(LIFE, x_mul(left, left)), FADE_FLOOR) >> 8;
        const int32_t size = x_mul(x_mul(scale >> 9, fade) >> 8, PUFF_SIZE) >> 4;
        v->puff(v->ctx, size, x_add(screen[i][0], offset[0]), x_add(screen[i][1], offset[1]));
    }
}
