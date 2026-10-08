/* Fixed-point maths of the race code. Like the original, it relies on >> of a
 * negative value being an arithmetic shift (true on every compiler we target).
 *
 * Trigonometry: Angles are 10-bit (0x400 per
 * turn); callers holding 24-bit angles (0x1000000 per turn, e.g. node heading
 * << 10) shift right by 14 first. Results are 16.16. */
#ifndef TNFS_FIXED_MATH_H
#define TNFS_FIXED_MATH_H

#include <stdint.h>

enum { FX_ANGLE_TURN = 0x400, FX_ONE = 0x10000 };

int32_t fx_sin(uint32_t angle);
int32_t fx_cos(uint32_t angle);
int32_t fx_sin16(int32_t angle);  /* 00472fc0, 0x10000 per turn */
int32_t fx_cos16(int32_t angle);  /* 00473030 */

/* 16.16 helpers of the 3D code. Matrices are 3x3, row major; vectors are
 * rows (v * m). */
int32_t fx_mul(int32_t a, int32_t b);
int32_t fx_div(int32_t a, int32_t b);
int32_t fx_atan2(int32_t y, int32_t x);  /* 004890f0, 16-bit angles */
void mat3_mul(const int32_t a[9], const int32_t b[9], int32_t out[9]);   /* 00475d4c */
void mat3_apply(const int32_t v[3], const int32_t m[9], int32_t out[3]); /* 00475da8 */

#endif
