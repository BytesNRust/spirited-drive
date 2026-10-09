#include "game/race/rigid_body.h"

#include "game/sys/fixed_math.h"
#include "game/sys/rigid_math.h"

#include <stddef.h>
#include <string.h>

enum {
    TICK = 0x888,                 /* 1/30 s */
    RADIANS_TO_ANGLE = 0x28be63,  /* 2^24 / 2pi */
    FALL = 0x53b6,                /* gravity per tick */
    HALF_ONE = 0x10000
};

typedef char car_body_place_check[offsetof(Car, body_matrix) == CAR_BODY_OFFSET &&
                                          offsetof(Car, speed_factor) + 4 == CAR_BODY_OFFSET + RIGID_BODY_SIZE
                                      ? 1 : -1];

void car_get_body(const Car *car, RigidBody *body)
{
    memcpy(body, (const uint8_t *)car + CAR_BODY_OFFSET, sizeof *body);
}

void car_set_body(Car *car, const RigidBody *body)
{
    memcpy((uint8_t *)car + CAR_BODY_OFFSET, body, sizeof *body);
}

/* a * (a >> 2) >> 14: a square at 2.14 */
static int32_t square14(int32_t a)
{
    return (a * (a >> 2)) >> 14;
}

static int32_t scale14(int32_t a, int32_t s)
{
    return ((a >> 2) * s) >> 14;
}

/* 1 / ((|v|^2 + 1) / 2): near 1 / |v| for a vector near unit length */
static int32_t unit_scale(int32_t a, int32_t b, int32_t c)
{
    return rm_inverse((square14(a) + square14(b) + square14(c) + HALF_ONE) >> 1);
}

void rb_orthonormalise(int32_t m[9])
{
    const int32_t s = unit_scale(m[0], m[1], m[2]);
    const int32_t a0 = scale14(m[0], s), a1 = scale14(m[1], s), a2 = scale14(m[2], s);
    const int32_t c0 = ((a1 * (m[5] >> 2)) >> 14) - ((a2 * (m[4] >> 2)) >> 14);
    const int32_t c1 = ((a2 * (m[3] >> 2)) >> 14) - (((m[5] >> 2) * a0) >> 14);
    const int32_t c2 = (((m[4] >> 2) * a0) >> 14) - (((m[3] >> 2) * a1) >> 14);
    const int32_t s2 = unit_scale(c0, c1, c2);
    const int32_t z0 = scale14(c0, s2), z1 = scale14(c1, s2), z2 = scale14(c2, s2);
    m[0] = a0, m[1] = a1, m[2] = a2;
    m[3] = ((z1 * (a2 >> 2)) >> 14) - ((z2 * (a1 >> 2)) >> 14);
    m[4] = ((z2 * (a0 >> 2)) >> 14) - (((a2 >> 2) * z0) >> 14);
    m[5] = (((a1 >> 2) * z0) >> 14) - (((a0 >> 2) * z1) >> 14);
    m[6] = z0, m[7] = z1, m[8] = z2;
}

/* 0042ebbc: 24-bit angle of (y over x) */
static int32_t angle_of(int32_t x, int32_t y)
{
    return fx_atan2(y, x) << 8;
}

void rb_axis_turn(int32_t out[9], int32_t angle, const int32_t axis[3])
{
    int32_t a[9], t[9], r[9];
    rm_turn_y(a, angle_of(axis[0], axis[2]));
    rm_turn_z(t, -angle_of(rm_length_xz(axis), axis[1]));
    rm_mul(a, a, t);
    rm_turn_x(r, angle);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) t[i * 3 + j] = a[j * 3 + i];
    rm_mul(out, a, r);
    rm_mul(out, out, t);
}

void rb_step(RigidBody *body)
{
    for (int i = 0; i < 3; i++) body->position[i] += fx_mul_round(body->velocity[i], TICK);
    for (int i = 0; i < 3; i++) body->velocity[i] -= body->velocity[i] >> 6;
    body->velocity[1] -= FALL;
    const int32_t turn = fx_mul_round(fx_mul_round(rm_length(body->spin), TICK), RADIANS_TO_ANGLE);
    int32_t r[9];
    rb_axis_turn(r, turn, body->spin);
    rm_mul(body->matrix, body->matrix, r);
}
