/* Fixed-point trigonometry of the race code. Angles are 10-bit (0x400 per
 * turn); callers holding 24-bit angles (0x1000000 per turn, e.g. node heading
 * << 10) shift right by 14 first. Results are 16.16. */
#ifndef TNFS_FIXED_MATH_H
#define TNFS_FIXED_MATH_H

#include <stdint.h>

enum { FX_ANGLE_TURN = 0x400, FX_ONE = 0x10000 };

int32_t fx_sin(uint32_t angle);
int32_t fx_cos(uint32_t angle);

#endif
