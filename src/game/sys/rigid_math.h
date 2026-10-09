/* The 3D maths of the rigid body (car crashes and the computer cars):
 * 16.16 vectors and 3x3 matrices with rounded products (imul, shrd 16,
 * adc), as the original's helpers at 0041d7fc..0041e000. */
#ifndef TNFS_RIGID_MATH_H
#define TNFS_RIGID_MATH_H

#include <stdint.h>

/* 0041d7fc */
void rm_identity(int32_t m[9]);

/* 0041d90c, 0041d8a4, 0041d83c: turns about x, y and z by a 24-bit angle. */
void rm_turn_x(int32_t m[9], int32_t angle);
void rm_turn_y(int32_t m[9], int32_t angle);
void rm_turn_z(int32_t m[9], int32_t angle);

/* 0041ddb0: about x, then z, then y. */
void rm_turn_xzy(int32_t m[9], int32_t x, int32_t z, int32_t y);

/* 0041dff4: out = a * b; out may be a or b. */
void rm_mul(int32_t out[9], const int32_t a[9], const int32_t b[9]);

/* 0041df3c: out = v * m (a row vector); out must not be v. */
void rm_apply(int32_t out[3], const int32_t v[3], const int32_t m[9]);

/* 0041df00 */
int32_t rm_dot(const int32_t a[3], const int32_t b[3]);

/* 0042eb54: Newton's square root of a 16.16 value from `guess`, until a
 * step moves less than 2^(16 - bits); 0 for 0. */
int32_t rm_sqrt(int32_t value, int32_t bits, int32_t guess);

/* 0042ebcc: 1 / x for an unsigned 16.16 x on the x87, whose divide runs
 * at single precision (24 significant bits, to nearest, ties to even)
 * before frndint truncates: 2^32 / x so rounded, then truncated. Past
 * 32 bits (x below 2) fistp gives 0x80000000. */
int32_t rm_inverse(int32_t x);

/* 0041d3b8: a vector's length, halving it first while it is large. */
int32_t rm_length(const int32_t v[3]);

/* 0041d4b8: the same for the vector's x and z only. */
int32_t rm_length_xz(const int32_t v[3]);

/* 0041d570: the squared length, halving large vectors first as 0041d3b8. */
int32_t rm_length_squared(const int32_t v[3]);

/* 0041d33c: scaled to length 1 (at 8.8 precision); left alone when 0. */
void rm_normalise(int32_t v[3]);

/* 0046ff3c: the matrix turned so its middle row is `up`: the first row
 * becomes up x last (normalised), the last up x first (8.8 products). */
void rm_set_up(int32_t m[9], const int32_t up[3]);

#endif
