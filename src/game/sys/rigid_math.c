#include "game/sys/rigid_math.h"

#include "game/sys/fixed_math.h"

enum {
    ONE = 0x10000,
    LENGTH_LIMIT = 0x1000000,     /* 0041d3b8 halves larger components */
    SQUARES_LIMIT = 0x40000000,
    SQRT_STEPS = 64               /* the original loops until it settles */
};

void rm_identity(int32_t m[9])
{
    static const int32_t I[9] = { ONE, 0, 0, 0, ONE, 0, 0, 0, ONE };
    for (int i = 0; i < 9; i++) m[i] = I[i];
}

void rm_turn_x(int32_t m[9], int32_t angle)
{
    const int32_t s = fx_sin16(angle >> 8), c = fx_cos16(angle >> 8);
    const int32_t r[9] = { ONE, 0, 0, 0, c, s, 0, -s, c };
    for (int i = 0; i < 9; i++) m[i] = r[i];
}

void rm_turn_y(int32_t m[9], int32_t angle)
{
    const int32_t s = fx_sin16(angle >> 8), c = fx_cos16(angle >> 8);
    const int32_t r[9] = { c, 0, -s, 0, ONE, 0, s, 0, c };
    for (int i = 0; i < 9; i++) m[i] = r[i];
}

void rm_turn_z(int32_t m[9], int32_t angle)
{
    const int32_t s = fx_sin16(angle >> 8), c = fx_cos16(angle >> 8);
    const int32_t r[9] = { c, s, 0, -s, c, 0, 0, 0, ONE };
    for (int i = 0; i < 9; i++) m[i] = r[i];
}

void rm_mul(int32_t out[9], const int32_t a[9], const int32_t b[9])
{
    int32_t r[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            r[i * 3 + j] = (int32_t)((uint32_t)fx_mul_round(a[i * 3], b[j]) +
                                     (uint32_t)fx_mul_round(a[i * 3 + 1], b[3 + j]) +
                                     (uint32_t)fx_mul_round(a[i * 3 + 2], b[6 + j]));
    for (int i = 0; i < 9; i++) out[i] = r[i];
}

void rm_turn_xzy(int32_t m[9], int32_t x, int32_t z, int32_t y)
{
    int32_t t[9];
    rm_turn_x(m, x);
    rm_turn_z(t, z);
    rm_mul(m, m, t);
    rm_turn_y(t, y);
    rm_mul(m, m, t);
}

void rm_apply(int32_t out[3], const int32_t v[3], const int32_t m[9])
{
    for (int j = 0; j < 3; j++)
        out[j] = (int32_t)((uint32_t)fx_mul_round(v[0], m[j]) + (uint32_t)fx_mul_round(v[1], m[3 + j]) +
                           (uint32_t)fx_mul_round(v[2], m[6 + j]));
}

int32_t rm_dot(const int32_t a[3], const int32_t b[3])
{
    return (int32_t)((uint32_t)fx_mul_round(a[2], b[2]) + (uint32_t)fx_mul_round(a[0], b[0]) +
                     (uint32_t)fx_mul_round(a[1], b[1]));
}

int32_t rm_sqrt(int32_t value, int32_t bits, int32_t guess)
{
    if (value == 0) return 0;
    int32_t shift = 16 - bits;
    if (shift < 0) shift = 0;
    if (shift > 16) shift = 16;
    uint32_t root = (uint32_t)guess;
    for (int step = 0; step < SQRT_STEPS; step++) {
        const uint32_t last = root;
        root = (last + (uint32_t)fx_div(value, (int32_t)last)) >> 1;
        if (((last - root) >> shift) == 0) break;
    }
    return (int32_t)root;
}

static int32_t abs32(int32_t v)
{
    return v > 0 ? v : -v;
}

static uint32_t square(int32_t v)
{
    return (uint32_t)fx_mul_round(v, v);
}

int32_t rm_length(const int32_t v[3])
{
    int32_t x = v[0], y = v[1], z = v[2], halvings = 0;
    while (abs32(x) > LENGTH_LIMIT || abs32(y) > LENGTH_LIMIT || abs32(z) > LENGTH_LIMIT) {
        x >>= 1, y >>= 1, z >>= 1;
        halvings++;
    }
    while ((square(z) >> 2) + (square(x) >> 2) + (square(y) >> 2) > SQUARES_LIMIT) {
        x >>= 1, y >>= 1, z >>= 1;
        halvings++;
    }
    const int32_t sum = (int32_t)(square(x) + square(y) + square(z));
    uint32_t length = (uint32_t)rm_sqrt(sum, 8, sum);
    for (; halvings != 0; halvings--) length *= 2;
    return (int32_t)length;
}

int32_t rm_length_xz(const int32_t v[3])
{
    int32_t x = v[0], z = v[2], halvings = 0;
    while (abs32(x) > LENGTH_LIMIT || abs32(z) > LENGTH_LIMIT) {
        x >>= 1, z >>= 1;
        halvings++;
    }
    while ((square(z) >> 2) + (square(x) >> 2) > SQUARES_LIMIT) {
        x >>= 1, z >>= 1;
        halvings++;
    }
    const int32_t sum = (int32_t)(square(z) + square(x));
    uint32_t length = (uint32_t)rm_sqrt(sum, 8, sum);
    for (; halvings != 0; halvings--) length *= 2;
    return (int32_t)length;
}

int32_t rm_length_squared(const int32_t v[3])
{
    int32_t x = v[0], y = v[1], z = v[2], halvings = 0;
    while (abs32(x) > LENGTH_LIMIT || abs32(y) > LENGTH_LIMIT || abs32(z) > LENGTH_LIMIT) {
        x >>= 1, y >>= 1, z >>= 1;
        halvings++;
    }
    while ((square(z) >> 2) + (square(x) >> 2) + (square(y) >> 2) > SQUARES_LIMIT) {
        x >>= 1, y >>= 1, z >>= 1;
        halvings++;
    }
    uint32_t sum = square(x) + square(y) + square(z);
    for (; halvings != 0; halvings--) sum <<= 2;
    return (int32_t)sum;
}

void rm_normalise(int32_t v[3])
{
    const int32_t sum = (v[1] >> 8) * (v[1] >> 8) + (v[0] >> 8) * (v[0] >> 8) + (v[2] >> 8) * (v[2] >> 8);
    const int32_t length = rm_sqrt(sum, 8, sum);
    if (length == 0) return;
    const int32_t inverse = fx_div(ONE, length) >> 8;
    for (int i = 0; i < 3; i++) v[i] = (v[i] >> 8) * inverse;
}

int32_t rm_inverse(int32_t x)
{
    const uint64_t u = (uint32_t)x;
    if (u < 2) return INT32_MIN;
    /* 2^32 / u (>= 1) rounded to 24 significant bits: q * 2^-shift */
    int top = 63;
    const uint64_t whole = ((uint64_t)1 << 32) / u;
    while (!(whole >> top & 1)) top--;
    const int shift = 23 - top;
    uint64_t q, r, d;
    if (shift >= 0) {
        d = u;
        q = ((uint64_t)1 << (32 + shift)) / d;
        r = ((uint64_t)1 << (32 + shift)) % d;
    } else {
        d = u << -shift;
        q = ((uint64_t)1 << 32) / d;
        r = ((uint64_t)1 << 32) % d;
    }
    if (2 * r > d || (2 * r == d && (q & 1))) q++;
    const uint64_t value = shift >= 0 ? q >> shift : q << -shift;
    return value > INT32_MAX ? INT32_MIN : (int32_t)value;
}
