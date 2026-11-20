#include "raster/car_pose.h"

#include "formats/fixed_math.h"

/* 00491e60, 00491f30, 00491ec8: turns about x, z and y from 10-bit angles. */
static void turn_x(uint32_t a, int32_t m[9])
{
    const int32_t s = fx_sin(a), c = fx_cos(a);
    const int32_t r[9] = { 0x10000, 0, 0, 0, c, s, 0, -s, c };
    for (int i = 0; i < 9; i++) m[i] = r[i];
}

static void turn_z(uint32_t a, int32_t m[9])
{
    const int32_t s = fx_sin(a), c = fx_cos(a);
    const int32_t r[9] = { c, s, 0, -s, c, 0, 0, 0, 0x10000 };
    for (int i = 0; i < 9; i++) m[i] = r[i];
}

static void turn_y(uint32_t a, int32_t m[9])
{
    const int32_t s = fx_sin(a), c = fx_cos(a);
    const int32_t r[9] = { c, 0, -s, 0, 0x10000, 0, s, 0, c };
    for (int i = 0; i < 9; i++) m[i] = r[i];
}

/* 004591d4: z about x, then y. The angles reach the builders as 16-bit values >> 14 (pushed as ushort). */
void car_body_matrix(int32_t heading, int32_t angle_x, int32_t angle_z, int32_t out[9])
{
    int32_t x[9], z[9], y[9], zx[9];
    turn_x((uint16_t)((uint32_t)angle_x >> 14), x);
    turn_z((uint16_t)((uint32_t)angle_z >> 14), z);
    turn_y((uint16_t)((uint32_t)heading >> 14), y);
    mat3_mul(z, x, zx);
    mat3_mul(zx, y, out);
}
