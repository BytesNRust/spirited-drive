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
int32_t fx_mul_round(int32_t a, int32_t b);  /* imul, shrd 16, adc 0: rounded half up */
/* 0042eb40: two words halved as unsigned, multiplied rounded, times 4. */
int32_t fx_mul_halves(uint32_t a, uint32_t b);
int32_t fx_atan2(int32_t y, int32_t x);  /* 004890f0, 16-bit angles */
void mat3_mul(const int32_t a[9], const int32_t b[9], int32_t out[9]);   /* 00475d4c */
void mat3_apply(const int32_t v[3], const int32_t m[9], int32_t out[3]); /* 00475da8 */
int32_t fx_atan2_10(int32_t y, int32_t x);   /* 00484294, 10-bit angles (0x400 per turn) */
/* 0042ece8: (x, z) turned by a 24-bit angle (>> 8 to the 16-bit sine) */
void fx_rotate(int32_t x, int32_t z, int32_t angle, int32_t *out_x, int32_t *out_z);
void fx_cross(const int32_t a[3], const int32_t b[3], int32_t out[3]);  /* 0042ec50, rounded products */
void mat3_scale_x(int32_t m[9], int32_t x);   /* 00449634: diag(x, 1, 1) in 16.16 */
void mat3_scale_y(int32_t m[9], int32_t y);   /* 00449670: diag(1, y, 1) in 16.16 */
void mat3_rotate_x(int32_t m[9], int32_t a);  /* 00449520, a 24-bit angle */
void mat3_rotate_y(int32_t m[9], int32_t a);  /* 0044957c */
void mat3_rotate_z(int32_t m[9], int32_t a);  /* 004495d8 */

#endif
