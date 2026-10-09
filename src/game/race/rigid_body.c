#include "game/race/rigid_body.h"

#include "game/sys/fixed_math.h"
#include "game/sys/rigid_math.h"

#include <stddef.h>
#include <string.h>

enum {
    TICK = 0x888,                 /* 1/30 s */
    RADIANS_TO_ANGLE = 0x28be63,  /* 2^24 / 2pi */
    FALL = 0x53b6,                /* gravity per tick */
    HALF_ONE = 0x10000,
    BOUNCE = 0x7333,              /* 004b9144 */
    FRICTION = 0xcccc,            /* 004b9148 */
    GRAVITY = 0x9cf5c,            /* 9.81 */
    COLLIDE_BOUNCE = 0x8ccc       /* 004b9140 */
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

void rb_angles(const int32_t m[9], int32_t *roll, int32_t *pitch, int32_t *yaw)
{
    const int32_t heading = angle_of(m[0], -m[2]);
    const int32_t flat = fx_mul_round(m[2], m[2]) + fx_mul_round(m[0], m[0]);
    const int32_t climb = angle_of(rm_sqrt(flat, 8, fx_mul_round(m[0], m[0]) + fx_mul_round(m[2], m[2])), m[1]);
    int32_t a[9], b[9], side[3];
    rm_turn_y(a, -heading);
    rm_turn_z(b, -climb);
    rm_mul(a, a, b);
    rm_apply(side, &m[6], a);
    *roll = angle_of(side[2], -side[1]);
    *pitch = climb;
    *yaw = heading;
}

/* The tangential velocity: v less its part along n, scaled to unit
 * length (negated); `length` gets its size. */
static void slide_direction(int32_t t[3], const int32_t n[3], int32_t *length)
{
    *length = 0;
    if (t[0] == 0 && t[1] == 0 && t[2] == 0) return;
    const int32_t along = ((t[2] >> 8) * (n[2] >> 8) + ((t[0] >> 8) * (n[0] >> 8) + (t[1] >> 8) * (n[1] >> 8))) >> 8;
    t[0] -= (n[0] >> 8) * along;
    t[1] -= (n[1] >> 8) * along;
    t[2] -= along * (n[2] >> 8);
    const int32_t sum = (t[2] >> 8) * (t[2] >> 8) + (t[1] >> 8) * (t[1] >> 8) + (t[0] >> 8) * (t[0] >> 8);
    *length = rm_sqrt(sum, 8, sum);
    const int32_t s = -rm_inverse(*length);
    for (int i = 0; i < 3; i++) t[i] = fx_mul_round(s, t[i]);
}

int32_t rb_contact_impulse(RigidBody *body, const int32_t point[3], const int32_t velocity[3],
                           const int32_t normal[3])
{
    const int32_t *n = normal;
    const int32_t n0 = n[0] >> 8, n1 = n[1] >> 8, n2 = n[2] >> 8;
    const int32_t closing = (body->velocity[0] >> 8) * n0 + (body->velocity[1] >> 8) * n1 + (body->velocity[2] >> 8) * n2;
    int32_t r[3];
    for (int i = 0; i < 3; i++) r[i] = point[i] - body->position[i];
    const int32_t c[3] = { n2 * (r[1] >> 8) - (r[2] >> 8) * n1, (r[2] >> 8) * n0 - n2 * (r[0] >> 8),
                           (r[0] >> 8) * n1 - n0 * (r[1] >> 8) };
    const int32_t weight = (body->inverse_10 >> 1) + (fx_mul_round(rm_length_squared(c), body->inverse_0c) >> 1);
    const int32_t speed = -(closing + (c[0] >> 8) * (body->spin[0] >> 8) + (c[1] >> 8) * (body->spin[1] >> 8) +
                            (body->spin[2] >> 8) * (c[2] >> 8));
    const int32_t bounce = fx_mul_round(fx_div(speed, weight), BOUNCE);
    int32_t t[3] = { velocity[0], velocity[1], velocity[2] }, slide;
    slide_direction(t, n, &slide);
    /* friction: how much the direction can take; (r x t) x r, built in
     * place as the original does (its second row reads the first new one) */
    int32_t friction = fx_mul_round(FRICTION, bounce);
    const int32_t linear = fx_mul_round(friction, body->inverse_10), angular = fx_mul_round(friction, body->inverse_0c);
    const int32_t r0 = r[0] >> 8, r1 = r[1] >> 8, r2 = r[2] >> 8;
    const int32_t t0 = t[0] >> 8, t1 = t[1] >> 8, t2 = t[2] >> 8;
    const int32_t w1 = t2 * r0 - t0 * r2, w2 = t0 * r1 - t1 * r0;
    const int32_t u0 = (w1 >> 8) * r2 - (w2 >> 8) * r1;
    const int32_t u1 = (w2 >> 8) * r0 - r2 * (u0 >> 8);
    const int32_t u2 = (u0 >> 8) * r1 - (u1 >> 8) * r0;
    const int32_t total[3] = { fx_mul_round(linear, t[0]) + fx_mul_round(angular, u0),
                               fx_mul_round(linear, t[1]) + fx_mul_round(angular, u1),
                               fx_mul_round(linear, t[2]) + fx_mul_round(angular, u2) };
    const int32_t take = t1 * (total[1] >> 8) + (total[0] >> 8) * t0 + (total[2] >> 8) * t2;
    if (take > slide) friction = fx_mul_round(friction, fx_div(slide, take));
    const int32_t rub[3] = { fx_mul_round(friction, t[0]), fx_mul_round(friction, t[1]), fx_mul_round(friction, t[2]) };
    if (bounce <= 0) return bounce;
    int32_t push[3];
    for (int i = 0; i < 3; i++) push[i] = rub[i] + fx_mul_round(bounce, n[i]);
    for (int i = 0; i < 3; i++) body->velocity[i] += fx_mul_round(body->inverse_10, push[i]);
    const int32_t p0 = push[0] >> 8, p1 = push[1] >> 8, p2 = push[2] >> 8;
    const int32_t turn[3] = { r1 * p2 - r2 * p1, r2 * p0 - p2 * r0, r0 * p1 - r1 * p0 };
    for (int i = 0; i < 3; i++) body->spin[i] += fx_mul_round(body->inverse_0c, turn[i]);
    return bounce;
}

/* The body's reach along the normal for each of its half sizes. */
static int32_t reach(const int32_t m[9], int row, const int32_t n8[3], int32_t extent)
{
    const int32_t *a = &m[row * 3];
    return (((a[0] >> 8) * n8[0] + (a[1] >> 8) * n8[1] + (a[2] >> 8) * n8[2]) >> 8) * (extent >> 8);
}

/* A point's velocity on the body: v + w x r (8.8 products). */
static void point_velocity(const RigidBody *b, const int32_t r8[3], int32_t out[3])
{
    out[0] = (r8[2] * (b->spin[1] >> 8) - (b->spin[2] >> 8) * r8[1]) + b->velocity[0];
    out[1] = ((b->spin[2] >> 8) * r8[0] - r8[2] * (b->spin[0] >> 8)) + b->velocity[1];
    out[2] = ((b->spin[0] >> 8) * r8[1] - (b->spin[1] >> 8) * r8[0]) + b->velocity[2];
}

void rb_ground_contact(RigidBody *body, const int32_t normal[3], const int32_t point[3], RbContact *contact)
{
    contact->bounce = 0;
    const int32_t n8[3] = { normal[0] >> 8, normal[1] >> 8, normal[2] >> 8 };
    const int32_t *m = body->matrix, *e = body->extents;
    int32_t sign[3];
    for (int i = 0; i < 3; i++) sign[i] = reach(m, i, n8, e[i]) < 0 ? 1 : -1;
    const int32_t d[3] = { body->position[0] - point[0], body->position[1] - point[1], body->position[2] - point[2] };
    const int32_t height = n8[2] * (d[2] >> 8) + (d[0] >> 8) * n8[0] + n8[1] * (d[1] >> 8);
    contact->depth = reach(m, 2, n8, e[2]) * sign[2] + reach(m, 0, n8, e[0]) * sign[0] +
                     reach(m, 1, n8, e[1]) * sign[1] + height;
    if (contact->depth >= 0) return;
    int32_t corner[3];
    for (int j = 0; j < 3; j++)
        corner[j] = body->position[j] + (m[j] >> 8) * (e[0] >> 8) * sign[0] + (m[3 + j] >> 8) * (e[1] >> 8) * sign[1] +
                    (m[6 + j] >> 8) * (e[2] >> 8) * sign[2];
    const int32_t sink = contact->depth != 0 && -contact->depth > 0 ? -contact->depth : 0;
    int32_t out[3] = { 0, 0, 0 };
    if (sink != 0) {
        for (int i = 0; i < 3; i++) out[i] = fx_mul_round(sink >> 1, normal[i]);
        for (int i = 0; i < 3; i++) body->position[i] += out[i];
    }
    const int32_t r8[3] = { (corner[0] - body->position[0]) >> 8, (corner[1] - body->position[1]) >> 8,
                            (corner[2] - body->position[2]) >> 8 };
    int32_t v[3];
    point_velocity(body, r8, v);
    if ((v[2] >> 8) * n8[2] + n8[0] * (v[0] >> 8) + n8[1] * (v[1] >> 8) < 0) {
        const int32_t bounce = rb_contact_impulse(body, corner, v, normal);
        if (bounce > 0) contact->bounce = bounce;
        point_velocity(body, r8, contact->velocity);
    }
    if (sink != 0 && out[1] > 0) {
        const int32_t climb = fx_mul_round(out[1], GRAVITY);
        const int32_t speed2 = fx_mul_round(body->velocity[1], body->velocity[1]);
        if (climb * 2 <= speed2) {
            const int32_t left = speed2 + climb * -2;
            const int32_t speed = rm_sqrt(left, 8, left);
            body->velocity[1] = body->velocity[1] < 1 ? -speed : speed;
        } else {
            body->velocity[1] = 0;
        }
    }
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

/* the arm from the body's centre to the contact, and its cross with the normal */
static void hit_arm(const RigidBody *b, const int32_t point[3], const int32_t normal[3], int32_t r[3], int32_t c[3])
{
    for (int i = 0; i < 3; i++) r[i] = point[i] - b->position[i];
    fx_cross(r, normal, c);
}

/* the spin a push at r gives the body (inverse_0c times r x push) */
static void hit_spin(RigidBody *b, const int32_t r[3], const int32_t push[3], int32_t sign)
{
    int32_t t[3];
    fx_cross(r, push, t);
    for (int i = 0; i < 3; i++) b->spin[i] += sign * fx_mul_round(b->inverse_0c, t[i]);
}

int rb_bodies_collide(RaceState *race, RigidBody *a, RigidBody *b, int32_t car_a, int32_t car_b,
                      const int32_t point[3], const int32_t normal[3], int32_t *impulse)
{
    int32_t ra[3], rb[3], ca[3], cb[3], va[3], vb[3], sa[3], sb[3];
    hit_arm(a, point, normal, ra, ca);
    hit_arm(b, point, normal, rb, cb);
    memcpy(va, a->velocity, sizeof va), memcpy(vb, b->velocity, sizeof vb);
    memcpy(sa, a->spin, sizeof sa), memcpy(sb, b->spin, sizeof sb);
    const int32_t closing = rm_dot(vb, normal) - rm_dot(va, normal) - rm_dot(sa, ca) + rm_dot(sb, cb);
    const int32_t weight = (a->inverse_10 >> 1) + (b->inverse_10 >> 1) +
                           (fx_mul_round(rm_dot(ca, ca), a->inverse_0c) >> 1) +
                           (fx_mul_round(rm_dot(cb, cb), b->inverse_0c) >> 1);
    const int32_t size = fx_div(closing, weight);
    if (size < 0) return 0;
    const int32_t bounce = fx_mul_round(COLLIDE_BOUNCE, size);
    if (impulse != NULL) *impulse = bounce;
    int32_t push[3];
    for (int i = 0; i < 3; i++) push[i] = fx_mul_round(bounce, normal[i]);
    if (race->effects != 0 && (race->viewed == car_a || race->viewed == car_b) && race->sounds.jolt != NULL)
        race->sounds.jolt(race->sounds.user, car_a, car_b, push[0], push[1], push[2], bounce);
    for (int i = 0; i < 3; i++) a->velocity[i] += fx_mul_round(a->inverse_10, push[i]);
    for (int i = 0; i < 3; i++) b->velocity[i] -= fx_mul_round(b->inverse_10, push[i]);
    hit_spin(a, ra, push, 1);
    hit_spin(b, rb, push, -1);
    return 1;
}
