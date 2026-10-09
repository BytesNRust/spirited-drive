/* The collision body a crashing or computer-driven car is moved as (the
 * 0x94 bytes at car +0xc4, car +0xc0 points to it), and its motion
 * (0041daa0, 0041d664, 0041d974). */
#ifndef TNFS_RIGID_BODY_H
#define TNFS_RIGID_BODY_H

#include "game/race/car.h"
#include "game/race/race_state.h"

#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
    int32_t matrix[9];       /* +0x00 */
    int32_t position[3];     /* +0x24, z the other way from the car's */
    int32_t velocity[3];     /* +0x30 */
    int32_t spin[3];         /* +0x3c */
    int32_t force[3];        /* +0x48 */
    int32_t unknown_54[3];
    int32_t pdn_10, pdn_0c;  /* +0x60 */
    int32_t inverse_10, inverse_0c;
    int32_t radius;          /* +0x70 */
    int32_t extents[3];      /* +0x74 */
    int32_t countdown;       /* +0x80 */
    int32_t unknown_84;
    int32_t slide_timer;     /* +0x88 */
    int32_t coast_timer;     /* +0x8c */
    int32_t speed_factor;    /* +0x90 */
} RigidBody;
#pragma pack(pop)
enum { RIGID_BODY_SIZE = 0x94, CAR_BODY_OFFSET = 0xc4 };
typedef char rigid_body_size_check[sizeof(RigidBody) == RIGID_BODY_SIZE ? 1 : -1];

/* The body as stored in the car's record (car_struct.h names each word). */
void car_get_body(const Car *car, RigidBody *body);
void car_set_body(Car *car, const RigidBody *body);

/* 0041d664: the matrix made orthonormal again (2.14 sums, 0042ebcc). */
void rb_orthonormalise(int32_t m[9]);

/* 0041d974: a turn by `angle` about `axis`. */
void rb_axis_turn(int32_t out[9], int32_t angle, const int32_t axis[3]);

/* 0041dc58: the turns about z, x and y (24-bit) the matrix holds. */
void rb_angles(const int32_t m[9], int32_t *roll, int32_t *pitch, int32_t *yaw);

/* 0041b9f8: the impulse of a contact at `point` (moving at `velocity`)
 * against a surface with unit `normal`: a bounce along the normal and
 * friction across it. Returns the bounce's size (0040b912c keeps it when
 * positive); nothing is applied unless it pushes the body away. */
int32_t rb_contact_impulse(RigidBody *body, const int32_t point[3], const int32_t velocity[3],
                           const int32_t normal[3]);

/* What the last contact left behind (globals of the original). */
typedef struct {
    int32_t bounce;          /* 004b912c: the bounce impulse, 0 when none */
    int32_t depth;           /* 004b9130: the deepest corner's distance into the surface, negative when in */
    int32_t velocity[3];     /* 004f9614: that corner's velocity after the bounce */
} RbContact;

/* 0041b414: the body's box against the plane through `point` with unit
 * `normal`: the deepest corner is pushed half way out, bounces off, and a
 * rising body keeps no more speed than its climb allows. */
void rb_ground_contact(RigidBody *body, const int32_t normal[3], const int32_t point[3], RbContact *contact);

/* 0041c098: two bodies in contact at `point` (world), `normal` unit: the
 * impulse that pushes them apart along the normal, applied to velocity and
 * spin (b takes the opposite). Returns 0 when they are already parting (the
 * impulse is negative), else 1; `impulse` (004b9128, may be NULL) gets its
 * size. car_a / car_b are the cars' slots (+0x516), the jolt goes to the
 * followed one. */
int rb_bodies_collide(RaceState *race, RigidBody *a, RigidBody *b, int32_t car_a, int32_t car_b,
                      const int32_t point[3], const int32_t normal[3], int32_t *impulse);

/* 0041daa0: one tick of free flight: moved by its velocity, slowed, pulled
 * down, and turned by its spin. */
void rb_step(RigidBody *body);

#endif
