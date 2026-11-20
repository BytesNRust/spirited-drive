/* 32-bit integer behaviour of the original x86 code, spelled out so every
 * target gives the same result: two's-complement wrap on add, sub, mul and
 * shl, arithmetic shift right of negatives, idiv truncating toward zero. */
#ifndef TNFS_GFX_X86_INT_H
#define TNFS_GFX_X86_INT_H

#include <stdint.h>

static inline int32_t x_add(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static inline int32_t x_sub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static inline int32_t x_mul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static inline int32_t x_shl(int32_t a, int n) { return (int32_t)((uint32_t)a << n); }

/* sar: the sign fills from the left. */
static inline int32_t x_sar(int32_t a, int n)
{
    return a >= 0 ? (int32_t)((uint32_t)a >> n) : ~(int32_t)((uint32_t)~a >> n);
}

/* idiv with a 32-bit dividend. The original raises #DE on a zero divisor or
 * INT32_MIN / -1; the callers keep the divisor positive, so those never happen
 * and this returns 0 for them. */
static inline int32_t x_div(int32_t a, int32_t b)
{
    if (b == 0 || (a == INT32_MIN && b == -1)) return 0;
    return a / b;
}

/* 64-bit dividend (edx:eax) over a 32-bit divisor, quotient truncated to 32
 * bits. The original raises #DE when the quotient does not fit. */
static inline int32_t x_div64(int64_t a, int32_t b)
{
    if (b == 0) return 0;
    return (int32_t)(a / b);
}

/* x / 8 rounding toward zero, as the sar/sbb pair at 0048766b computes it. */
static inline int32_t x_div8(int32_t x) { return x_sar(x_add(x, x < 0 ? 7 : 0), 3); }

#endif
